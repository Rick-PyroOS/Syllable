using namespace graphics::theme::style;

class Style::Private{
    public:
        Private();
     public:
        graphics::FillStyle m_cFillStyle;
        graphics::FillStyle m_cBackgroundFillStyle;
        graphics::PenStyle m_cPenStyle;
        graphics::PenStyle m_cTextStyle;
        graphics::theme::style::BorderStyle m_cInnerBorderStyle;
        graphics::theme::style::BorderStyle m_cOuterBorderStyle;
        os::Rect m_cMargin;
        os::Rect m_cPadding;
};


Style::Style() {
    m = new Private;
    m->m_cFillStyle = graphics::FillSolid;
    m->m_cPenStyle = graphics::PenStyle("black");
    m->m_cBackgroundFillStyle = graphics::FillSolid;
    m->m_cInnerBorderStyle = graphics::theme::style::BorderStyle();
    m->m_cOuterBorderStyle = graphics::theme::style::BorderStyle();
    m->m_cMargin = os::Rect();
    m->m_cPadding = os::Rect();
}

void Style::SetFillStyle(const graphics::FillStyle& style) const{
    m->m_cFillStyle = style;
}

graphics::FillStyle Style::GetFillStyle() const{
    return m->m_cFillStyle;
}

void SetPenStyle(const graphics::PenStyle& style) const{
    m->m_cPenStyle = style;
}

graphics::PenStyle GetPenStyle(){
    return m->m_cPenStyle;
}

void Style::SetInnerBorderStyle(const graphics::theme::style::BorderStyle& style ){
    m->m_cInnerBorderStyle = style;
}

graphics::theme::style::BorderStyle Style::GetInnerBorderStyle() const{
    return m_cInnerBorderStyle;
 }

void Style::SetOuterBorderStyle(const graphics::theme::style::BorderStyle& style){
    m->m_cOuterBorderStyle = style;
}

graphics::theme::style::BorderStyle GetOuterBorderStyle() const{
    return m->m_cOuterBorderStyle;
}

 void Style::SetTextPenStyle(const graphics::PenStyle& style) const{
    m->m_cTextStyle = m_cTextStyle;
 }

graphics::PenStyle Style::GetTextPenStyle(){
    return m->m_cTextStyle;
}

void Style::SetBackgroundFillStyle(const graphics::FillStyle& style){
    m->m_cBackgroundFillStyle = style;
}

graphics::FillStyle Style::GetBackgroundFillStyle(){
    return m->m_cBackgroundFillStyle;
}

 os::Rect Style::GetMargin() const{
   return m->m_cMargin;
 }

 void Style::SetMargin(const os::Rect& rect){
    m->m_cMargin = rect;
 }

os::Rect Style::GetPadding() const{
    return m->m_cPadding;
}
void Style::SetPadding(const os::Rect& rect){
    m->m_cPadding = rect;
}
