#include "common.h"

os::Image* GetImageFromResource( os::String zResource, os::Point size )
{
	os::File cSelf(open_image_file( get_image_id()));
	os::Resources cCol(&cSelf);
	os::ResStream *pcStream = cCol.GetResourceStream(zResource);

	if (pcStream != NULL) {
		os::BitmapImage *pcImage = new os::BitmapImage();
		pcImage->Load(pcStream);
		pcImage->SetSize(size);
		delete (pcStream);
		return (pcImage);
	}
	return NULL;
}
