var x = 4;
let y = -x + 3 * 2;
const threshold = 10;
let metrics = getMetrics(x, y);
metrics = metrics.next();
let score = (metrics.value + metrics["offset"]) * threshold;
score = score + (typeof metrics === "object" ? 1 : 0);
let finalValue = (score = score + 1, score);
logResult(finalValue);
