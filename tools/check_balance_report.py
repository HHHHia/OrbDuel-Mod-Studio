"""Check that the recorded competitive run still describes the shipped content.

This checks saved evidence and acceptance bounds; it does not run new matches.
"""

import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "docs/balance/integrated.json"
CONTENT = ROOT / "content"
MAPS = ("open", "obstacles", "mechanisms", "shrinking")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def equal(actual, expected):
    if isinstance(actual, (int, float)) and not isinstance(actual, bool):
        return isinstance(expected, (int, float)) and math.isclose(
            actual, expected, rel_tol=1e-5, abs_tol=1e-4
        )
    return actual == expected


def check_score(score, games, label, low, high):
    require(score["games"] == games, f"{label}: wrong game count")
    wins, draws = score["wins"], score["draws"]
    require(0 <= wins <= games and 0 <= draws <= games - wins, f"{label}: invalid results")
    rate = 100 * (wins + draws / 2) / games
    require(math.isclose(rate, score["winRate"], abs_tol=1e-6), f"{label}: wrong score")
    require(low <= rate <= high, f"{label}: {rate:.2f}% outside {low}–{high}%")


def check_content(report):
    characters = {
        item["id"]: item
        for path in (CONTENT / "characters").glob("*/character.json")
        for item in [json.loads(path.read_text(encoding="utf-8"))]
    }
    eligible = {key for key, value in characters.items() if value.get("competitive", True)}
    excluded = set(characters) - eligible
    roster = {item["id"]: item for item in report["roster"]}
    require(len(report["roster"]) == len(roster) == 8 and set(roster) == eligible,
            "competitive roster changed")
    require(set(report["excludedCharacters"]) == excluded, "excluded roster changed")
    fields = (
        ("hp", "stats", "maxHp", 100),
        ("attack", "stats", "attack", 12),
        ("armor", "stats", "armor", 0),
        ("speed", "stats", "startSpeed", 420),
        ("mass", "stats", "mass", 1),
        ("massMax", "stats", "massMax", 0),
        ("massRamp", "stats", "massRamp", 90),
        ("restitution", "stats", "restitution", 0.95),
        ("contact", "hitbox", "contact", 1),
        ("ghost", "hitbox", "ghost", False),
        ("radius", "hitbox", "radius", 18),
        ("shape", "skin", "shape", "sprite"),
    )
    for key, saved in roster.items():
        current = characters[key]
        require(saved["name"] == current["name"]["zh"], f"{key}: name changed")
        require(saved["skills"] == current.get("skills", []), f"{key}: skills changed")
        for target, section, source, default in fields:
            value = current.get(section, {}).get(source, default)
            require(equal(saved[target], value), f"{key}: {target} changed")

    skills = {
        path.stem: json.loads(path.read_text(encoding="utf-8"))
        for path in (CONTENT / "skills").glob("*.json")
    }
    recorded = {item["id"]: item for item in report["skillDefinitions"]}
    require(recorded == skills, "skill definitions changed since measurement")


def check_results(report):
    require(report["firstSeed"] == 90000 and report["seedsPerMap"] == 128,
            "independent seed range changed")
    require(report["swappedSides"] is True and tuple(report["maps"]) == MAPS,
            "scenario setup changed")
    roster = {item["id"] for item in report["roster"]}
    seeds = report["seedsPerMap"]
    games = len(roster) * (len(roster) - 1) // 2 * len(MAPS) * seeds * 2
    require(report["games"] == games == 28672, "total game count changed")
    require(48 <= report["leftSideScore"] <= 52, "side advantage outside target")
    require(set(report["characters"]) == roster, "character results incomplete")
    require(set(report["matchups"]) == roster, "matchup results incomplete")
    require(set(report["byMap"]) == set(MAPS), "map results incomplete")
    for key in roster:
        check_score(report["characters"][key], (len(roster) - 1) * len(MAPS) * seeds * 2,
                    key, 40, 60)
        opponents = report["matchups"][key]
        require(set(opponents) == roster - {key}, f"{key}: matchup coverage incomplete")
        for rival, score in opponents.items():
            check_score(score, len(MAPS) * seeds * 2, f"{key} vs {rival}", 20, 80)
            reverse = report["matchups"][rival][key]
            require(score["wins"] + reverse["wins"] + score["draws"] == score["games"]
                    and score["draws"] == reverse["draws"],
                    f"{key} vs {rival}: asymmetric results")
        require(sum(score["wins"] for score in opponents.values())
                == report["characters"][key]["wins"], f"{key}: matchup wins disagree")
        require(sum(score["draws"] for score in opponents.values())
                == report["characters"][key]["draws"], f"{key}: matchup draws disagree")
        for map_name in MAPS:
            require(set(report["byMap"][map_name]) == roster,
                    f"{map_name}: character coverage incomplete")
            score = report["byMap"][map_name][key]
            check_score(score, (len(roster) - 1) * seeds * 2,
                        f"{map_name}/{key}", 20, 80)
        require(sum(report["byMap"][m][key]["wins"] for m in MAPS)
                == report["characters"][key]["wins"], f"{key}: map wins disagree")
        require(sum(report["byMap"][m][key]["draws"] for m in MAPS)
                == report["characters"][key]["draws"], f"{key}: map draws disagree")


if __name__ == "__main__":
    data = json.loads(REPORT.read_text(encoding="utf-8"))
    check_content(data)
    check_results(data)
    rates = [item["winRate"] for item in data["characters"].values()]
    print(f"Balance report OK: {data['games']} games, 8 competitive fighters, "
          f"score {min(rates):.2f}–{max(rates):.2f}%, "
          f"left side {data['leftSideScore']:.2f}%")
