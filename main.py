from datetime import datetime
import requests
from nyct_gtfs import NYCTFeed

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


def main():
    feed = NYCTFeed("N")  # library handles fetch + parse

    manhattan = get_minutes(feed, STOP_MANHATTAN)
    coney = get_minutes(feed, STOP_CONEY)

    print("N Train (Manhattan-Bound):")
    print(", ".join(f"{m} mins" for m in manhattan))
    print("-" * 40)
    print("N (Coney Island-Bound):")
    print(", ".join(f"{m} mins" for m in coney))


if __name__ == "__main__":
    main()