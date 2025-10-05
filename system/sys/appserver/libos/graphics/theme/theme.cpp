#include <graphics/theme/theme.h>
#include <cassert>

class Theme::Private{
public:
    os::String m_cFile;
    graphics::theme::style::Style m_cStyle;
    ThemeInfo m_cInfo;
}

Theme::Theme() {
    m = new Private;
    m->m_cStyle = NULL;
}

Theme::Theme(const os::String& cFile){
    assert(new os::FSNode(cFile).IsValid() == false);
    m->m_cFile = cFile;
    m->m_cStyle = new graphics::theme::style::Style();
    m->m_cInfo = ThemeInfo();
    __ReadFromFile__(cFile);
}

graphics::theme::style::Style Theme::GetStyle() const{
    return m->m_cStyle;
}

ThemeInfo Theme::GetInfo() const{
    return m->m_cInfo;
}

void Theme::__ReadFromFile(const os::File& file){
    Settings* settings = new os::Settings(file);
    m->m_cInfo.m_cAuthor = settings->GetString("author","");
    m->m_cInfo.m_cDescription = settings->GetString("description","");
    m->m_cInfo.m_cImageFile = settings->GetString("image","");
    m->m_cInfo.m_cStyleFile = settings->GetString("style","");
    assert(new os::FSNode(m->m_cInfo.m_cImageFile).IsValid() == false);
    assert(new os::FSNode(m->m_cInfo.m_cStyleFile).IsValid() == false);


}
