#ifndef SBS_BATTLE_CALLBACK_COPY_H
#define SBS_BATTLE_CALLBACK_COPY_H

// Audited opts into copying the callable as-is. It must use the supplied battle,
// own its captured mutable state by value, and not share mutable external state.
// This is a contract at the registration site, not a property of std::function.
enum class BattleCallbackCopy { Unverified, Audited };

#endif
