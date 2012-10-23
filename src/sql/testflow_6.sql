reset role;
SET client_min_messages = notice;
SET log_error_verbosity = terse;
-- SELECT ftruncatetables();
-- select fresetmarket();
SET role client;

-- two concurrent paths, (a) is the best
-- path (b)
-- select finsertorder('A','x',25  ,100 ,'z');
select finsertorder('A','z',200 ,200 ,'x');
-- path (a)
-- select finsertorder('B','x',25  ,200 ,'y');
select finsertorder('B','y',200 ,25  ,'x');
--select finsertorder('C','y',100 ,100 ,'z');
select finsertorder('C','z',100 ,100 ,'y');

-- no exchange
select id,uuid,nb,oruuid,grp,provider,quality,qtt,receiver from vmvt order by uuid;

--select finsertorder('D','z',250 ,250 ,'x');
select finsertorder('D','x',250 ,250 ,'z');
--two exchanges in a single transaction
select id,uuid,nb,oruuid,grp,provider,quality,qtt,receiver from vmvt order by uuid;

select * from fgetstats(true);
select * from fgeterrs();

