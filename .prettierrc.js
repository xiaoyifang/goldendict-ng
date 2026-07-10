module.exports = {
  printWidth: 80,
  tabWidth: 2,
  useTabs: false,
  trailingComma: "all",
  bracketSpacing: true,
  proseWrap: "preserve",
  htmlWhitespaceSensitivity: "ignore",
  endOfLine: "lf",
  embeddedLanguageFormatting: "auto",
  overrides: [
    {
      files: ["*.md", "**/*.md"],
      options: {
        tabWidth: 4,
        proseWrap: "preserve",
      },
    },
  ],
};
