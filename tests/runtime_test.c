/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: runtime_test.c
 * Description: Checks the EmuMD runtime on its own: the plugin
 *              interface, ROM3/ROM4, options, flash rules, FatFs on a
 *              host folder and core 1 as a thread.
 *
 *   runtime_test <empty scratch folder>
 */
#include <sys/stat.h>

#include "ff.h"
#include "hardware/flash.h"
#include "emumd_plugin.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

static int s_fails;
#define CHECK(c)                                                     \
  do {                                                               \
    if (!(c)) {                                                      \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);   \
      s_fails++;                                                     \
    }                                                                \
  } while (0)

/* ---- a tiny firmware ------------------------------------------------ */

static unsigned s_irqs, s_polls, s_samples;
static uint16_t s_last;

static void irq(void) {
  uint16_t v;
  s_irqs++;
  while (mdfw_rom3_pop(&v)) {
    s_samples++;
    s_last = v;
  }
}

static int app_init(void) {
  static const uint16_t cart[] = {0xabcd, 0xef42, 0x1234};
  mdfw_rom4_load(cart, 3);
  mdfw_rom3_set_irq(irq);
  return 0;
}

static bool app_poll(void) {
  s_polls++;
  return false;
}

const mdfw_app_t mdfw_app = {.name = "test", .version = "v0",
                             .init = app_init, .poll = app_poll};

static char s_log[4096];
static void host_log(const char *line) {
  strncat(s_log, line, sizeof(s_log) - strlen(s_log) - 2);
  strcat(s_log, "\n");
}

/* ---- core 1 --------------------------------------------------------- */

static void core1_echo(void) {
  for (;;) {
    const uint32_t v = multicore_fifo_pop_blocking();
    multicore_fifo_push_blocking(v * 2u + get_core_num());
  }
}

int main(int argc, char **argv) {
  const char *sd = argc > 1 ? argv[1] : "/tmp/emumd-test";
  char path[1024];
  mkdir(sd, 0777);
  snprintf(path, sizeof(path), "%s/Rott", sd);
  mkdir(path, 0777);

  const emumd_plugin_t *p = emumd_plugin_v1();
  CHECK(p->abi == EMUMD_PLUGIN_ABI);
  CHECK(strcmp(p->name, "test") == 0);
  const emumd_host_t host = {EMUMD_PLUGIN_ABI, sd, "a=1\nb=hello world\nflag\nb=last\nempty=",
                             1, host_log};
  CHECK(p->power_on(&host, 1000) == 0);

  /* Options */
  CHECK(mdfw_option_int("a", 0) == 1);
  CHECK(strcmp(mdfw_option("b"), "last") == 0);
  CHECK(strcmp(mdfw_option("flag"), "1") == 0);
  CHECK(strcmp(mdfw_option("empty"), "") == 0);
  CHECK(mdfw_option("missing") == NULL);

  /* ROM4 / ROM3 / time */
  CHECK(p->rom4_read(0, 2000) == 0xabcd);
  CHECK(p->rom4_read(4, 2000) == 0x1234);
  p->rom3_read(0x8765, 3000);
  CHECK(s_irqs == 1 && s_samples == 1 && s_last == 0x8765);
  CHECK(time_us_64() == 3000);
  sleep_ms(5);
  CHECK(time_us_64() == 8000 && timer_hw->timerawl == 8000);
  p->tick(4000);
  CHECK(s_polls > 0);
  mdfw_log("logged %d", 42);
  CHECK(strstr(s_log, "test: logged 42") != NULL);

  /* Flash */
  CHECK(mdfw_flash[0] == 0xff);
  uint8_t page[FLASH_PAGE_SIZE];
  memset(page, 0x0f, sizeof(page));
  flash_range_program(0x1000, page, sizeof(page));
  memset(page, 0xf3, sizeof(page));
  flash_range_program(0x1000, page, sizeof(page));
  CHECK(mdfw_flash[0x1000] == 0x03); /* bits only clear */
  flash_range_erase(0x1000, FLASH_SECTOR_SIZE);
  CHECK(mdfw_flash[0x1000] == 0xff);
  CHECK((uintptr_t)&mdfw_flash[0x2000] - XIP_BASE == 0x2000);

  /* FatFs: create, ignore case, append, list, stat, rename, delete */
  FATFS fs;
  FIL f;
  UINT n;
  char buf[64];
  CHECK(f_mount(&fs, "", 1) == FR_OK);
  CHECK(f_open(&f, "0:/ROTT/Test.TXT", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK);
  CHECK(f_write(&f, "hello", 5, &n) == FR_OK && n == 5);
  CHECK(f_close(&f) == FR_OK);
  CHECK(f_open(&f, "/rott/test.txt", FA_WRITE | FA_OPEN_APPEND) == FR_OK);
  CHECK(f_printf(&f, " %s", "world") == 6);
  f_close(&f);
  CHECK(f_open(&f, "/rott/TEST.TXT", FA_READ) == FR_OK);
  CHECK(f_size(&f) == 11);
  CHECK(f_read(&f, buf, sizeof(buf), &n) == FR_OK && n == 11);
  buf[n] = 0;
  CHECK(strcmp(buf, "hello world") == 0 && f_eof(&f));
  f_close(&f);
  CHECK(f_open(&f, "/rott/none.txt", FA_READ) == FR_NO_FILE);
  CHECK(f_open(&f, "/nowhere/x.txt", FA_READ) == FR_NO_PATH);
  DIR d;
  FILINFO fi;
  CHECK(f_opendir(&d, "/ROTT") == FR_OK);
  CHECK(f_readdir(&d, &fi) == FR_OK && strcmp(fi.fname, "Test.TXT") == 0 &&
        fi.fsize == 11 && !(fi.fattrib & AM_DIR));
  CHECK(f_readdir(&d, &fi) == FR_OK && fi.fname[0] == 0);
  f_closedir(&d);
  CHECK(f_chdir("/rott") == FR_OK);
  CHECK(f_stat("test.txt", &fi) == FR_OK && fi.fsize == 11);
  CHECK(f_rename("test.txt", "renamed.txt") == FR_OK);
  CHECK(f_unlink("/Rott/RENAMED.TXT") == FR_OK);
  CHECK(f_stat("renamed.txt", &fi) == FR_NO_FILE);

  /* Core 1 as a thread, and the FIFOs both ways */
  multicore_launch_core1(core1_echo);
  for (uint32_t i = 1; i <= 100; i++) {
    multicore_fifo_push_blocking(i);
    CHECK(multicore_fifo_pop_blocking() == i * 2u + 1u);
  }
  uint32_t v;
  CHECK(!multicore_fifo_pop_timeout_us(1000, &v));
  p->power_off(); /* stops core 1 */

  printf("runtime_test: %s\n", s_fails ? "FAIL" : "OK");
  return s_fails ? 1 : 0;
}
