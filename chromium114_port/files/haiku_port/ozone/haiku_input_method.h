#ifndef RCHROMIUM_HAIKU_INPUT_METHOD_H_
#define RCHROMIUM_HAIKU_INPUT_METHOD_H_

#include <Messenger.h>

#include "haiku_ime_event.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Turns Haiku input method events into a composition on the focused text
// input client, so that pages get compositionstart/update/end and keydown
// with key "Process" (keyCode 229) and isComposing -- the way they do on
// other systems. Keys the input method lets through are handled as
// InputMethodMinimal handles them.
class InputMethodHaiku : public InputMethodBase {
 public:
  InputMethodHaiku(ImeKeyEventDispatcher* ime_key_event_dispatcher,
                   gfx::AcceleratedWidget widget);
  InputMethodHaiku(const InputMethodHaiku&) = delete;
  InputMethodHaiku& operator=(const InputMethodHaiku&) = delete;
  ~InputMethodHaiku() override;

  // The input method of `widget`'s window, or null. UI thread only.
  static InputMethodHaiku* ForWidget(gfx::AcceleratedWidget widget);

  void OnHaikuImeEvent(const HaikuImeEvent& event);

  // InputMethod:
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;

 protected:
  // InputMethodBase:
  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) override;

 private:
  void DispatchProcessKey();
  // Asks the Haiku input method to drop its composition: the text stays
  // where Chromium put it, and the method starts afresh.
  void StopHaikuComposition();

  gfx::AcceleratedWidget widget_;
  bool composing_ = false;
  BMessenger reply_to_;
};

}  // namespace ui

#endif  // RCHROMIUM_HAIKU_INPUT_METHOD_H_
