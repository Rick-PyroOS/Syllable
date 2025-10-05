#ifndef __GRAPHICS_THEME_THEME_H__
#define __GRAPHICS_THEME_THEME_H__

#include "style/style.h"
#include "style/border.h"

#include <storage/file.h>
namespace graphics {
    namespace theme {
        struct ThemeInfo {
            os::String m_cAuthor;
            os::String m_cDescription;
            os::String m_cStyleFile;
            os::String m_cImageFile;
        };
        class Theme
        {
            public:
                Theme();
                graphics::theme::style::Style GetStyle() const;
                ThemeInfo GetThemeInfo() const;
            private:
                class Private;
                Private *m;

                void __ReadFromFile(const os::String& cFile);
                void __Init__();
        };
    }
}

const graphics::theme::Theme DEFAULT_THEME = graphics::theme::Theme(os::File("/system/themes/default/style.css"));


#endif