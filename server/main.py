from flask import Flask, jsonify
from datetime import datetime
from nyct_gtfs import NYCTFeed

app = Flask(__name__)

STOP_MANHATTAN = "N02N"
STOP_CONEY = "N02S"

def get_minutes(feed, stop_id):
    now = datetime.now()
    times = []
    for trip in feed.filter_trips(headed_for_stop_id=stop_id):
        for s in trip.stop_time_updates:
            if s.stop_id == stop_id and s.arrival:
                mins = max(0, round((s.arrival - now).total_seconds() / 60))
                times.append(mins)
    return sorted(times)[:4]

@app.route('/subway')
def subway_data():
    try:
        feed = NYCTFeed("N")
        manhattan = get_minutes(feed, STOP_MANHATTAN)
        coney = get_minutes(feed, STOP_CONEY)
        
        # Returns a beautifully clean, tiny JSON object for your ESP32
        return jsonify({
            "manhattan": manhattan,
            "coney": coney
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == "__main__":
    # host="0.0.0.0" makes the server visible to other devices on your Wi-Fi
    app.run(host="0.0.0.0", port=5000)