# Anki Integration



# prerequisite
1. install anki
2. install ankiconnect

# configure anki

## 1. create a new model, or use an existing model

For example, the model could have `Front` and `Back` fields.

![Snipaste_2022-05-21_14-08-21](https://user-images.githubusercontent.com/105986/169638410-c6aa8038-df03-40de-8731-9f0b9f43bf59.png)
## 2. configure the template
the front template

![image](https://user-images.githubusercontent.com/105986/169638457-2358d020-0132-469f-a6b4-0fb6d1590fa2.png)
the back template

![image](https://user-images.githubusercontent.com/105986/169638440-7191fcdd-c338-48a3-a899-7216a5c77425.png)

# configure goldendict
## 1. through toolbar=>preference=>network
![screenshot](https://user-images.githubusercontent.com/69171671/224496944-dbf31d6e-26be-42c9-98fc-257f70a8428e.png)

* Word - Vocabulary headword.
* Text - Selected definition.
* Sentence - Search string. You can leave it blank.

**Example for adding Japanese sentences:**
![screenshot](https://user-images.githubusercontent.com/69171671/224497112-ab027a16-89b2-48d8-8308-a3dbb5b9e1e4.png)

## 2. action
![image](https://user-images.githubusercontent.com/105986/169638740-abecde84-d33b-45ce-932c-d465c6650334.png)
## 3. result

**Word and definition:**
![image](https://user-images.githubusercontent.com/105986/169638761-f67c009d-27cd-440d-bafa-ebbdce9577e3.png)

**Sentence, word, and definition:**
![screenshot](https://user-images.githubusercontent.com/69171671/224497528-889d6393-e04d-4af7-b1a7-816ba010f2b2.png)



## Using URI schemes

`goldendict://word` link can be use to query a word directly on Goldendict.

On your Anki card's template, you can add the code below to have a "1 click open in Goldendict" card.

```
<a href="goldendict://{{Front}}">{{Front}}</a>
```

Note that this feature doesn't available on macOS 