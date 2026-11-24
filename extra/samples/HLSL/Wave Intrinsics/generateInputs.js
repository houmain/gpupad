var firstValue = 1
var inputs = []
for (var i = 0; i < 64; ++i)
    inputs.push(firstValue + i)
app.session.getItem("Inputs").setData(inputs)
