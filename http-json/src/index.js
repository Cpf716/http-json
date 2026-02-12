// Corey Ferguson
// January 28, 2026
// http-json
// index.js
//

fetch('http://localhost:8080/api/greeting', {
    method: 'post',
    body: JSON.stringify({
        firstName: "Corey"
    }),
    headers: {
        'content-type': 'application/json'
    }
}).then(response =>
    response.json().then(value => console.log(value))
).catch(err => {
    throw err;
})
