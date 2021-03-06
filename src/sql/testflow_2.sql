drop extension if exists flowf cascade;
create extension flowf;
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

-- order limit lnNoQttLimit (4+1+128)
SELECT yflow_show('[(1,1,1,1,10,20,100,7.00),(133,2,2,1,10,20,0,7.00)]'::yflow); -- omega >1

-- order limit lnNoQttLimit+lnIgnoreOmega (8+4+1+128)
SELECT yflow_show('[(1,1,1,1,10,20,100,7.00),(141,2,2,1,0,0,0,7.00)]'::yflow); -- omega >1
SELECT yflow_qtts('[(1,1,1,1,10,20,100,7.00),(141,2,2,1,0,0,0,7.00)]'::yflow); -- omega >1
