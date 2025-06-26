#include "custom_font_combobox.hh"

CustomFontComboBox::CustomFontComboBox(QWidget *parent)
    : QFontComboBox(parent)
{
}

void CustomFontComboBox::insertCustomItem(int index ,const QString & item)
{
    if (index < 0 || index > this->count()) {
        index = 0; // Default to the beginning if index is out of bounds
    }
    insertItem(index, item);
}
