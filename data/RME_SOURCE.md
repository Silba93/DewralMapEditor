# RME data provenance

The built-in DME material profiles were generated from:

- Repository: https://github.com/OTAcademy/RME
- Commit: `5b179c21ff5ae57ccefd1df97ff4264cac8b0340`
- Commit date: 2026-01-28
- Converter: `scripts/import-rme-data.py`

Generated files contain text material definitions only. CipSoft DAT/SPR files,
RME `items.otb` files, and client artwork are not copied.

## Profile mapping

| DME profile | RME material directory |
|---|---|
| 7.60 | 760 |
| 7.72 | 760 (fallback) |
| 7.80 | 760 (fallback) |
| 7.92 | 760 (fallback) |
| 8.00 | 800 |
| 8.10 | 810 |
| 8.20 | 820 |
| 8.40 | 840 |
| 8.50 | 850 |
| 8.54 | 854 |
| 8.60 | 860 |
| 8.70 | 870 |
| 9.10 | 910 |
| 9.20 | 920 |
| 9.46 | 946 |
| 9.54 | 954 |
| 9.60 | 960 |
| 9.86 | 986 |
| 10.10 | 1010 |
| 10.30 | 1030 |
| 10.41 | 1041 |
| 10.77 | 1077 |
| 10.98 | 1098 |

RME does not provide separate material directories for 7.72, 7.80, or 7.92.
Using 7.60 for those profiles avoids introducing item IDs that only exist in
the 8.00 data set.
