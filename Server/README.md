# GA-based Planner for Kriti PS

Usage:

1. Ensure `test_case_excel.txt` is in the same folder as `solver.py` (already provided).
2. Run:

```bash
python solver.py "test_case_excel.txt"
```

What it does:
- Parses the provided tab-sheet formatted test file.
- Runs a simple genetic algorithm to assign employees to available vehicles and sequence pickups.
- Prints the best assignment found, objective score, total cost and aggregated employee ride time.

Notes:
- This is a straightforward GA implementation intended as a working baseline. It enforces capacity and basic sharing preferences, estimates travel using haversine distance and vehicle speeds, and penalizes constraint violations.
- You can tune GA parameters by editing `solver.py` (population size, generations).
