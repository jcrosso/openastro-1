/*****************************************************************************
 *
 * QHYfirmware.h -- header for QHY firmware management
 *
 * Copyright 2014 James Fidell (james@openastroproject.org)
 *
 * License:
 *
 * This file is part of the Open Astro Project.
 *
 * The Open Astro Project is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Open Astro Project is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Open Astro Project.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef OA_QHY_FIRMWARE_H
#define OA_QHY_FIRMWARE_H

extern int	oaQHYCameraDeviceHasLoadableFirmware ( oaCameraDevice* );
extern int	oaQHYCameraDeviceIsFirmwareLoaded ( oaCameraDevice* );
extern int	oaQHYCameraDeviceLoadFirmware ( oaCameraDevice* );

#endif	/* OA_QHY_FIRMWARE_H */

