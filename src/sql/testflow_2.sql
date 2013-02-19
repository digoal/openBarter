/*drop schema IF EXISTS test CASCADE;
CREATE SCHEMA test;
SET search_path TO test;

create extension flow with version '1.0';

RESET client_min_messages;
RESET log_error_verbosity;
SET client_min_messages = notice;
SET log_error_verbosity = terse; 
*/
-- yflow ''[(type,id,oid,own,qtt_requ,qtt_prov,qtt,proba), ...]''

-- order limit
SELECT yflow_show('[(1,1,1,1,10,20,20,7.00),(1,2,2,1,10,20,20,7.00)]'::yflow); -- omega >1
SELECT yflow_show('[(1,1,1,1,20,20,20,7.00),(1,2,2,1,20,20,20,7.00)]'::yflow); -- omega =1
SELECT yflow_show('[(1,1,1,1,20,10,10,7.00),(1,2,2,1,20,10,10,7.00)]'::yflow); -- omega <1

-- order best
SELECT yflow_show('[(2,1,1,1,10,20,20,7.00),(2,2,2,1,10,20,20,7.00)]'::yflow); -- omega >1
SELECT yflow_show('[(2,1,1,1,20,20,20,7.00),(2,2,2,1,20,20,20,7.00)]'::yflow); -- omega =1
SELECT yflow_show('[(2,1,1,1,20,10,10,7.00),(2,2,2,1,20,10,10,7.00)]'::yflow); -- omega <1

-- order limit and best
SELECT yflow_show('[(1,1,1,1,20,10,10,7.00),(2,2,2,1,20,10,10,7.00)]'::yflow); -- omega <1
SELECT yflow_show('[(2,1,1,1,20,10,10,7.00),(1,2,2,1,20,10,10,7.00)]'::yflow); -- omega <1

-- order limit lnNoQttLimit (4+1)
SELECT yflow_show('[(1,1,1,1,10,20,100,7.00),(5,2,2,1,10,20,0,7.00)]'::yflow); -- omega >1

-- order limit lnNoQttLimit+lnIgnoreOmega (8+4+1)
SELECT yflow_show('[(1,1,1,1,10,20,100,7.00),(13,2,2,1,0,0,0,7.00)]'::yflow); -- omega >1
SELECT yflow_qtts('[(1,1,1,1,10,20,100,7.00),(13,2,2,1,0,0,0,7.00)]'::yflow); -- omega >1