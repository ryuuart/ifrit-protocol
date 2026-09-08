/** @file
 * Qt's key and the text it types, read as p5's name and number.
 */

#include "Keys.h"

#include <QtCore/Qt>
#include <QtGui/QKeySequence>

std::pair<std::string, int> keyAs(int qtKey, const QString& text) {
  switch (qtKey) {
    case Qt::Key_Left:
      return {"ArrowLeft", 37};
    case Qt::Key_Up:
      return {"ArrowUp", 38};
    case Qt::Key_Right:
      return {"ArrowRight", 39};
    case Qt::Key_Down:
      return {"ArrowDown", 40};
    case Qt::Key_Return:
    case Qt::Key_Enter:
      return {"Enter", 13};
    case Qt::Key_Escape:
      return {"Escape", 27};
    case Qt::Key_Backspace:
      return {"Backspace", 8};
    case Qt::Key_Delete:
      return {"Delete", 46};
    case Qt::Key_Tab:
      return {"Tab", 9};
    case Qt::Key_Space:
      return {" ", 32};
    case Qt::Key_Shift:
      return {"Shift", 16};
    case Qt::Key_Control:
      return {"Control", 17};
    case Qt::Key_Alt:
      return {"Alt", 18};
    case Qt::Key_Meta:
      return {"Meta", 91};
    case Qt::Key_Home:
      return {"Home", 36};
    case Qt::Key_End:
      return {"End", 35};
    case Qt::Key_PageUp:
      return {"PageUp", 33};
    case Qt::Key_PageDown:
      return {"PageDown", 34};
    case Qt::Key_Insert:
      return {"Insert", 45};
    default:
      break;
  }
  if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12)
    return {"F" + std::to_string(qtKey - Qt::Key_F1 + 1),
            112 + (qtKey - Qt::Key_F1)};
  // The character it types — but only when it typed one a reader would
  // recognise: a chord over a letter types a control character, and a
  // sketch asked to compare against \x01 has been told which key was
  // pressed in a way it cannot use. Qt's own key is that letter.
  if (!text.isEmpty() && text.at(0).unicode() >= 0x20) {
    const QChar first = text.at(0);
    const int code =
        first.isLetter() ? first.toUpper().unicode() : (int)first.unicode();
    return {text.toStdString(), code};
  }
  if (qtKey >= 0x20 && qtKey <= 0x7e) {
    const QChar typed = QChar(qtKey);
    return {QString(typed.toLower()).toStdString(),
            (int)typed.toUpper().unicode()};
  }
  return {QKeySequence(qtKey).toString().toStdString(), qtKey};
}
