/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2557-codec.h
**
** Description:
**     header file for tas2557-codec.c
**
** =============================================================================
*/

#ifndef _TAS2557_REGMAP_H
#define _TAS2557_REGMAP_H

#include "tas2557.h"

int tas2557_i2c_probe(struct i2c_client *pClient, const struct i2c_device_id *pID);
int tas2557_i2c_remove(struct i2c_client *pClient);


#endif /* _TAS2557_CODEC_H */
