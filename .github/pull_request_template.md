## Summary

- What changed:
- Why:

## Contract and Support Boundary

- [ ] I verified whether this change alters any documented support claim in [`docs/reference/HostCompatibility.md`](../docs/reference/HostCompatibility.md).
- [ ] If behavior/support changed, I updated docs and tests in the same PR.

## Validation

- [ ] `python tools/check_contract_sync.py`
- [ ] `python tools/check_schema_keyword_coverage.py`
- [ ] `pio -d firmware test -e teensy40_unity -vvv` (or clearly documented why not)
- [ ] `npm --prefix bridge test`
- [ ] `npm --prefix App test`

## Risk Notes

- Areas with highest regression risk:
- Rollback plan:
