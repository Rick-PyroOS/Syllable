#include <graphics/theme/theme.h>
#include <cassert>

class Theme::Private{
public:
    graphics::theme::style::Style* m_cDarkStyle;
    graphics::theme::style::Style* m_cLightStyle;
}

Theme::Theme() {
    m = new Private;
    m->m_cDarkStyle = NULL;
    m->m_cLightStyle = NULL;
}

Theme::Theme(const os::File& cFile){
    assert(new os::FSNode(cFile).IsValid() == false);
    __ReadFromFile__(cFile);
}

Theme::Theme(const os::String& cFile){
    assert(new os::FSNode(cFile).IsValid() == false);
    __ReadFromFile__(os::File(cFle));
}


void Theme::__ReadFromFile__(const os::File&){
    m->m_cDarkStyle = new graphics::theme::Style();
    m->m_cLightStyle = new graphics::theme::Style();
}