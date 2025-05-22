## Add external programs or scripts

GoldenDict can use programs or scripts as dictionary.

### An example

Tokenize the input german sentence into words.

#### Install Python and SoMaJo.

You can follow the instructions in the documentation https://github.com/tsproisl/SoMaJo?tab=readme-ov-file#installation

```
pip install -U SoMaJo
```

#### Create a script

```
from somajo import SoMaJo
import sys

tokenizer = SoMaJo("de_CMC", split_camel_case=True)

# note that paragraphs are allowed to contain newlines
paragraphs = [sys.argv[1]]
sentences = tokenizer.tokenize_text(paragraphs)
for sentence in sentences:
    for token in sentence:
        if token.token_class=='regular':
            print(f"{token.text}")
```

and save the script in the `E:\test.py` for example.

#### Add the script to the program dictionary

![Add the script as program dictionary](add-program-dictionary.png)
`python e:\test.py %GDWORD%`

#### The result

![](program-result.png)



