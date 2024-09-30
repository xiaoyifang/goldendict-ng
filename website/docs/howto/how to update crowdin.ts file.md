This project uses crowdin to organize all the transactions.When some new transactions items are added.The crowdin.ts file needs to be updated to reflect the changes.   Then crowdin will automatically generate all other transactions in different languages(Though all these translations still need to be translated manually or by Machine Translation).

# how to update the crowdin.ts file

```
lupdate.exe -no-obsolete -no-ui-lines -locations none .\src\   -ts .\locale\crowdin.ts
```

the option `-no-obsolete`  will remove obsolete items from crowdin.ts file.

the option `-no-ui-lines` will not generate line numbers in the crowdin.ts file.  The line numbers changes too often which will make too much code changes  between commits.