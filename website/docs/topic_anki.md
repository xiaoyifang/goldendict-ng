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

![screenshot](https://user-images.githubusercontent.com/69171671/224497112-ab027a16-89b2-48d8-8308-a3dbb5b9e1e4.png)

### Action

![image](https://user-images.githubusercontent.com/105986/169638740-abecde84-d33b-45ce-932c-d465c6650334.png)

### Result

#### Word and definition

![image](https://user-images.githubusercontent.com/105986/169638761-f67c009d-27cd-440d-bafa-ebbdce9577e3.png)

#### Sentence, word, and definition

![screenshot](https://user-images.githubusercontent.com/69171671/224497528-889d6393-e04d-4af7-b1a7-816ba010f2b2.png)

## Using URI schemes

The `goldendict://word` link can be used to query a word directly in GoldenDict-ng.

On your Anki card's template, you can add the code below to create a "1-click open in GoldenDict-ng" link:

```html
<a href="goldendict://{{Front}}">{{Front}}</a>
```
