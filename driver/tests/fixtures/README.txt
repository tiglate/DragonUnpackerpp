payload_m1.rnc / payload_m2.rnc: the payload of make_fixtures.py (RNC_PAYLOAD)
packed with RNC ProPack method 1 and 2 by the reference tool
(rnc_propack_source, `rnc p in out -m=1|2`). make_fixtures.py checks nothing about
them: the test compares what the driver unpacks with RNC_PAYLOAD.
