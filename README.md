# HTS - It's HTML, but fells like CSS

HTS is a small CSS-like language that's transpiled to HTML.

It's not intent to be used in real applications as it is a little bit weird. But it was a cool weekend project to work with.

## How do it works
Using the same idea behind CSS nested selectors, where you can declare selectors inside each other.
```css
parent-tag {
  child-tag-1 {}
  child-tag-2 {}
}
```

You can use id and classes with the same sintax of CSS:
```css
div#container.shown {}
```

> [!TIP]
Is only possible to declare tag's id once, and always before classes. Ex: tag-name#id.class1.class2 ....

Tag bodies can assume 3 forms:
```css
/* Block based: Allows declaration of attributes and children */
div {
  contenteditable="true";

  "Text content"
} 

/* String body: Use for single-line/text only elements (like buttons, spans, etc) */
h1 "My tittle"

/* Multiline content blocks: Can be used for tags like pre, style, script, and for using regular html inside hts */
style [
  body { 
    display: flex;
    flex-direction: column;
    background: red;
  }
]
```

Note that, when using hts blocks, the first declaration should be attribute definiton, using the following syntax:
```css
link {
  href="css/styles.css"; /* comma are optional */
  rel="stylesheet"
  type="text/css"
}
```

After attributes definition you begin children declaration, which can be strings, multiline content blocks or regular hts tags.
```css
div {
  "This is a string"
    
  br {} // this is a tag
    
  [
    This is
    multiline text
    inside a tag
  ]
}
```

## Example

```css
html {
  lang="en"

  head {
    meta { charset="UTF-8" }
    title "HTS Example"
  }

  body {
    h1 "Hello world"
    p "This is an paragraph made in HTS"

    button#ok-btn "Ok"

    script [
        document.querySelector("#ok-btn").onclick = () => alert("OK has been clicked")
    ]
  }
}
```
