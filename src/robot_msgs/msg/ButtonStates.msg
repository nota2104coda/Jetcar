# ButtonStates.msg
# auto_mode : 0 = manual, 1 = auto
bool auto_mode_button
# stop : 0 = go, 1 = stop
bool stop_button

# Motion intents. At most one should be true at a time, but the
# receiving node must decide how to prioritize conflicts.
bool forward
bool backward
bool left_turn
bool right_turn
