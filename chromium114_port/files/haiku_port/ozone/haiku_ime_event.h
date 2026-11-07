#ifndef RCHROMIUM_HAIKU_IME_EVENT_H_
#define RCHROMIUM_HAIKU_IME_EVENT_H_

#include <Messenger.h>
#include <SupportDefs.h>

#include <string>

namespace ui {

// What a Haiku input method sent the content view (B_INPUT_METHOD_EVENT),
// carried from the BWindow looper to the UI thread. Header of its own so that
// the RTTI half (beapi_views) does not need the IME headers.
struct HaikuImeEvent {
  int32 opcode = 0;  // B_INPUT_METHOD_STARTED, _CHANGED or _STOPPED
  std::u16string text;
  bool confirmed = false;
  // The message came straight from a key the method took: koreanime marks
  // the first message of each key with "koreanime:key". Methods that do not
  // mark keys (marks_keys false) get every unconfirmed change counted as one.
  bool from_key = false;
  bool marks_keys = false;
  BMessenger reply_to;
};

}  // namespace ui

#endif  // RCHROMIUM_HAIKU_IME_EVENT_H_
