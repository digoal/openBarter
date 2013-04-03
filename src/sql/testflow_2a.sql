/*drop schema IF EXISTS test CASCADE;
CREATE SCHEMA test;
SET search_path TO test;

create extension flowf; -- with version '1.0';

RESET client_min_messages;
RESET log_error_verbosity;
SET client_min_messages = notice;
SET log_error_verbosity = terse; */

-- yflow ''[(type,id,oid,own,qtt_requ,qtt_prov,qtt,proba), ...]''


-- (type,id,oid,own,qtt_requ,qtt_prov,qtt,proba)
select yflow_show('[(2, 8928, 8928, 72, 49263, 87732, 87732, 1.000000),(1, 515, 515, 69, 53751, 67432, 67432, 1.000000),(141, 10001, 10001, 72, 1, 1, 1, 1.000000)]'::yflow);
select yflow_show('[(2, 8928, 8928, 72, 49263, 87732, 87732, 1.000000),(1, 515, 515, 69, 53751, 67432, 67432, 1.000000),(1, 10001, 10001, 72, 67432, 30183,30183, 1.000000)]'::yflow);
