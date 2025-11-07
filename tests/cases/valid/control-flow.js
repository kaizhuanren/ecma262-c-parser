let limit = 10;
let count = 0;
for (let i = 0; i < limit; i++) {
  if (i % 2 === 0) {
    continue;
  }
  count = count + i;
  if (count > 12) {
    break;
  }
}
if (count >= 10) {
  count = count - 5;
} else {
  count = count + 1;
}
