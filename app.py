from flask import Flask, request, render_template

app = Flask(__name__)

location = {"lat": 0.0, "lon": 0.0}

@app.route('/')
def live_location():
    return render_template("index.html", lat=location["lat"], lon=location["lon"])

@app.route('/update', methods=['POST'])
def update_location():
    location["lat"] = request.form.get("lat")
    location["lon"] = request.form.get("lon")
    return "Location updated", 200

if __name__ == '__main__':
    app.run(debug=True, host="0.0.0.0", port=5000)
