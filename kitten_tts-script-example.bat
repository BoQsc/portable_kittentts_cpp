@ECHO OFF
:: Rosie - Large NPC Dialogue Set

:: Greetings and General Interaction
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "Ho there, traveler! Careful where you step. You're currently standing in the shadow of the greatest warrior this province has ever seen."
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "You look like you've walked a hundred miles and eaten half as many meals. Sit. The bench is reinforced for a man of my carriage, so it will certainly hold you."
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "State your business, and speak up! The air is a bit thinner up here, and I haven't got all day to lean down."

:: Quest and Lore (Large Volume)
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "You want to hear of the Great Siege? It was a time of massive iron and even larger egos. I carried the battering ram myself when the oxen gave out. We didn't just break the gates; we turned them into splinters for the evening fire."
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "The beast you seek is gargantuan, true. But remember—the bigger they are, the more noise they make when I eventually drop my hammer on their skull."


:: Combat Barks
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "That's it? I've had more intimidating scratches from a house cat!"
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "Make way! I'm coming through, and I don't plan on stopping for anyone smaller than a barn door!"
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "You fought well for a little spark, but now you face the forest fire."

:: Dismissal
kitten_tts.exe --session avatar --model nano --speaker Rosie --text "Move along now. These boots are heavy, and I'd hate to ruin your day by misplacing my foot."

kitten_tts.exe --session avatar --terminate
PAUSE