// DiceBot — simple dice roller

function parseDice(input) {
    if (!input) return { count: 1, sides: 6 };

    var match = input.match(/^(\d+)d(\d+)$/i);
    if (!match) {
        var n = parseInt(input);
        if (!isNaN(n) && n >= 1 && n <= 100) return { count: 1, sides: n };
        return null;
    }

    var count = parseInt(match[1]);
    var sides = parseInt(match[2]);

    if (count < 1 || count > 20 || sides < 2 || sides > 1000) return null;
    return { count: count, sides: sides };
}

function rollDice(count, sides) {
    var rolls = [];
    for (var i = 0; i < count; i++) {
        rolls.push(Math.floor(Math.random() * sides) + 1);
    }
    return rolls;
}

grotto.onCommand("/roll", {
    description: "Roll NdN dice",
    usage: "/roll [NdN]"
}, function(ctx) {
    var dice = parseDice(ctx.args[0]);
    if (!dice) {
        grotto.sendMessage(ctx.channel,
            ctx.sender + ": Invalid format. Usage: /roll 2d6");
        return;
    }

    var rolls = rollDice(dice.count, dice.sides);
    var total = 0;
    for (var i = 0; i < rolls.length; i++) total += rolls[i];

    if (dice.count === 1) {
        grotto.sendMessage(ctx.channel,
            ctx.sender + ": d" + dice.sides + " = " + total);
    } else {
        grotto.sendMessage(ctx.channel,
            ctx.sender + ": " + dice.count + "d" + dice.sides + " = " + rolls.join(" + ") + " = " + total);
    }
});

grotto.log.info("DiceBot loaded!");
