import requests

from nyct_gtfs import NYCTFeed

STOP_MANHATTAN = "N02N"  # toward Manhattan
STOP_CONEY = "N02S"      # toward Coney Island

def fetch_feed():
    feed = NYCTFeed("N", fetch_immediately=False)
    r = requests.get(NYCTFeed._train_to_url["N"], timeout=10)
    r.raise_for_status()
    feed.load_gtfs_bytes(r.content)
    return feed

def minutes_list(feed, stop_id):
    now = datetime.now()
    mins = []

    for trip in feed.filter_trips(headed_for_stop_id=stop_id):
        stop = next(
            (s for s in trip.stop_time_updates if s.stop_id == stop_id),
            None
        )
        if not stop or not stop.arrival:
            continue
        mins.append(
            max(0, round((stop.arrival - now).total_seconds() / 60))
        )

    return sorted(mins)[:4]

def get_display_text():
    feed = fetch_feed()
    manhattan = minutes_list(feed, STOP_MANHATTAN)
    coney = minutes_list(feed, STOP_CONEY)

    lines = ["N Train (Manhattan-Bound):", ", ".join(f"{m} mins" for m in manhattan), "-" * 40, "N (Coney Island-Bound):", ", ".join(f"{m} mins" for m in coney),]

    return "\n".join(lines)

if __name__ == "__main__":
    print(get_display_text())