# Anki Integration

## Prerequisites

- Install Anki
- Install AnkiConnect add-on for Anki

## Configure Anki

### Create a new model or use an existing model

For example, the model could have `Front` and `Back` fields.

![Anki Setting](img/anki-1.png)

### Configure the template

#### Front template

![Anki Front Template](img/anki-front.png)

#### Back template

![Anki Back Template](img/anki-back.png)

## Configure GoldenDict

### Through toolbar → Preferences → Anki tab

![Anki Goldendict Configure](img/anki-gd.png)

#### Field mappings
- **Word**: Vocabulary headword
- **Text**: Selected definition
- **Sentence**: Search string (can be left blank)

#### Example for adding Japanese sentences

![screenshot](img/anki-jp-example.png)

### Action

![image](img/anki-action.png)

### Result

#### Word and definition

![image](img/anki-result-word.png)

#### Sentence, word, and definition

![screenshot](img/anki-result-sentence.png)

## Using URI schemes

The `goldendict://word` link can be used to query a word directly in GoldenDict-ng.

On your Anki card's template, you can add the code below to create a "1-click open in GoldenDict-ng" link:

```html
<a href="goldendict://{{Front}}">{{Front}}</a>
```
