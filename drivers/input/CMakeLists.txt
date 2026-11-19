# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_library_amend()

zephyr_library_sources_ifdef(CONFIG_INPUT_AZOTEQ_IQS5XX iqs5xx.c)

target_sources_ifdef(CONFIG_ZMK_INPUT_AZOTEQ_IQS5XX_IDLE_SLEEPER app PRIVATE zmk_azoteq_idle_sleeper.c)
