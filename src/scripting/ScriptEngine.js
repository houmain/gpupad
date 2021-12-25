
(function() {
  function output(level) {
    return function() {
       var text = ''
       for (var i = 0, n = arguments.length; i < n; i++) {
         text += (i == 0 ? '' : ' ')
         if (typeof arguments[i] === 'object')
           text += JSON.stringify(arguments[i], null, 2)
         else
           text += arguments[i]
       }
       console.output(text, level)
     }
  }
  console.log = output(0)
  console.warn = output(1)
  console.error = output(2)
})();
