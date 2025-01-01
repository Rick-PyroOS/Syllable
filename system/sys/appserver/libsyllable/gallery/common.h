#ifndef __COMMON_H_
#define __COMMON_H_

#include <gui/image.h>
#include <util/resource.h>
#include <util/resources.h>
#include <util/string.h>
#include <storage/file.h>
#include "defines.h"

os::Image* GetImageFromResource( os::String zResource,const os::Point size = os::Point(24,24) );

#endif
