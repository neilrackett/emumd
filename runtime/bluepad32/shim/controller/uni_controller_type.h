/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's controller/uni_controller_type.h (the
 * models SDL's controller_type.h lists, with Bluepad32's own after them):
 * the same values. A scripted pad is a GenericController. */
#ifndef MDFW_SHIM_UNI_CONTROLLER_TYPE_H
#define MDFW_SHIM_UNI_CONTROLLER_TYPE_H
typedef enum {
  k_eControllerType_None = -1,
  k_eControllerType_Unknown = 0,
  k_eControllerType_UnknownSteamController = 1,
  k_eControllerType_SteamController = 2,
  k_eControllerType_SteamControllerV2 = 3,
  k_eControllerType_SteamControllerNeptune = 4,
  k_eControllerType_UnknownNonSteamController = 30,
  k_eControllerType_XBox360Controller = 31,
  k_eControllerType_XBoxOneController = 32,
  k_eControllerType_PS3Controller = 33,
  k_eControllerType_PS4Controller = 34,
  k_eControllerType_WiiController = 35,
  k_eControllerType_AppleController = 36,
  k_eControllerType_AndroidController = 37,
  k_eControllerType_SwitchProController = 38,
  k_eControllerType_SwitchJoyConLeft = 39,
  k_eControllerType_SwitchJoyConRight = 40,
  k_eControllerType_SwitchJoyConPair = 41,
  k_eControllerType_SwitchInputOnlyController = 42,
  k_eControllerType_MobileTouch = 43,
  k_eControllerType_XInputSwitchController = 44,
  k_eControllerType_PS5Controller = 45,
  k_eControllerType_XInputPS4Controller = 46,
  k_eControllerType_iCadeController = 50,
  k_eControllerType_SmartTVRemoteController = 51,
  k_eControllerType_8BitdoController = 52,
  k_eControllerType_GenericController = 53,
  k_eControllerType_NimbusController = 54,
  k_eControllerType_OUYAController = 55,
  k_eControllerType_PSMoveController = 56,
  k_eControllerType_AtariJoystick = 57,
  k_eControllerType_LastController,
  k_eControllertype_GenericKeyboard = 400,
  k_eControllertype_GenericMouse = 800,
} uni_controller_type_t;
#endif
