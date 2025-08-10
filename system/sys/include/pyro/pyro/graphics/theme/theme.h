#ifndef __GRAPHICS_THEME_THEME_H__
#define __GRAPHICS_THEME_THEME_H__

#include "style/style.h"
#include "style/border.h"

#include <storage/file.h>
namespace graphics {
    namespace theme {
        class Theme
        {
            public:
                Theme();
                Theme(const os::File&);

                graphics::theme::style::Style GetDarkMode() const{
                    return m->m_cDarkStyle;
                }
                graphics::theme::style::Style GetLightMode() const{
                    return m->m_cLightStyle;
                }
            private:
                class Private;
                Private *m;

                void __ReadFromFile__(const os::File&);
                void __Init__();
        };
    }
}

const graphics::theme::Theme DEFAULT_THEME = graphics::theme::Theme(os::File("/system/themes/.default/style.css"));


#endif