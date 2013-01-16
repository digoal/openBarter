
#include "postgres.h"
#include "float.h"

#include <float.h>
#include <math.h> 
#include "wolf.h"

#include "lib/stringinfo.h"

TresChemin *wolfc_maximum(Twolf *wolf);
char * wolfc_vecIntStr(short dim,int64 *vecInt);


static double _calOmega(TresChemin *chem);
static void _calOwns(TresChemin *chem);
static void _calGains(TresChemin *chem);
static short _wolfMaximum(TresChemin *chem);
static Tstatusflow _rounding(short iExhausted, TresChemin *chem);


/******************************************************************************
 * gives the maximum flow of wolf
 * return chem->flowNodes[.] and chem->status
 * flowNodes is copied to wolf->x[.].flowr
 *****************************************************************************/
TresChemin *wolfc_maximum(Twolf *wolf) {
	short _dim = wolf->dim;
	short _i; 
	short _iExhausted;
	TresChemin *chem;

	chem = (TresChemin *) palloc(sizeof(TresChemin));

	chem->status = empty;
	chem->lastignore = false;
	chem->wolf = wolf;
	
	if(_dim == 0) 
		goto _end;	

	if(_dim > FLOW_MAX_DIM) 
		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			 errmsg("max dimension %i reached for the flow",FLOW_MAX_DIM)));
 
	chem->lastignore = wolf->x[_dim-1].qtt_requ == 0;
		
	obMRange(_i,_dim) 
		if(wolf->x[_i].qtt == 0) 
			goto _dropflow; // box->status = empty
		else if(wolf->x[_i].qtt < 0) 
			ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("flow with qtt <0")));
				 
	// elog(WARNING,"flowc_maximum: dim=%i",_dim);
	if(_dim == 1) {
		chem->status = noloop;
		goto _dropflow;
	}

	if(!follow_orders(&wolf->x[_dim-1],&wolf->x[0])) {
		chem->status = noloop; 
		goto _dropflow;
	}
	// it is a cycle <= end->begin

				 
	//elog(WARNING,"flowc_maximum: ->status!=noloop");
	// error for 3-agreement: on omega ~= 1.1e-16
	
	if( _calOmega(chem) < 1.0) {
		chem->status = refused;
		goto _dropflow;
	}
		
	_calOwns(chem);

	// omegas are bartered so that the product(omegaCorrige[i]) for i in [0,_dim-1])== 1 
	_calGains(chem); // computes opegaCorrige[i]

	/* maximum flow, as a floating point vector 
	in order than at least one stock is exhausted by the flow
	and than the ratio between successive elements of flowr is omegaCorrige */
	_iExhausted = _wolfMaximum(chem); 
	
	/* the floating point vector is rounded 
	to the nearest vector of positive integers */
	chem->status = _rounding(_iExhausted, chem);
	if(chem->status == draft)
		goto _end;
	
_dropflow: 	
	{
		short _k;
	
		// the flow is undefined
		obMRange (_k,_dim)
			wolf->x[_k].flowr = -1;		
	}
		
_end:
	// elog(WARNING,"chem: %s",wolfc_cheminToStr(chem));
	return chem;
}

/******************************************************************************
 * 
 *****************************************************************************/
static double _calOmega(TresChemin *chem) {
	Twolf *wolf = chem->wolf;
	short _n,_dim = wolf->dim;
	double *omega = chem->omega;
		
	chem->prodOmega = 1.0;
	
	obMRange(_n,_dim) {		
		if((_n == _dim-1) && chem->lastignore) {
			continue;
		} else {
			Torder *b = &wolf->x[_n];

			// compute omega and prodOmega
			if(b->qtt_prov == 0 || b->qtt_requ == 0) {
				ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("_calOmega: qtt_prov or qtt_requ is zero for Torder[%i]",_n)));
			}		 
			omega[_n] = ((double)(b->qtt_prov)) / ((double)(b->qtt_requ));
			chem->prodOmega *= omega[_n];
		}
	}
	if (chem->lastignore) {
		/* omega of the last node is not defined.
		It is set in order than the product of omegas becomes 1.0 :
		omega[_dim-1] 	= 1./product(omega[i] for i in [0,_dim-2] )
		*/
		omega[_dim-1] = 1.0/chem->prodOmega;
		chem->prodOmega = 1.0;
	}
	return chem->prodOmega;
}

/******************************************************************************
 * 
 *****************************************************************************/
static void _calOwns(TresChemin *chem) {
	
	short *occOwn = chem->occOwn; 
	short _n;
	Twolf *wolf = chem->wolf;
	short _dim = wolf->dim;
	bool _res;		
	
	// defines ownIndex and updates nbOwn
	/****************************************************/
	chem->nbOwn = 0;
	obMRange(_n,_dim) {
		short _m;
		short _ownIndex = chem->nbOwn;		
		Torder *b = &wolf->x[_n];
		short *ownIndex = chem->ownIndex;
			
		obMRange(_m,_ownIndex) {
			
			IDEMTXT(b->own,wolf->x[_m].own,_res);
			// elog(WARNING,"own1: %s,own2: %s",follow_DatumTxtToStr(b->own),follow_DatumTxtToStr(wolf->x[_m].own));
			if (_res) {
				_ownIndex = _m;
				break;// found
			}
		}
		if (_ownIndex == chem->nbOwn) { // not found
			occOwn[_ownIndex] = 0;
			chem->nbOwn += 1;
		}
		occOwn[_ownIndex] += 1;
		ownIndex[_n] = _ownIndex;	
	} 
	

	// sanity checks
	/****************************************************/	
	if(chem->nbOwn == 0) { 
		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			 errmsg("createChemin: nbOwn equals zero")));		
	}
	 
	#ifdef GL_VERIFY
	obMRange(_n,_dim) {
		short _m;
		
		Torder *o = &wolf->x[_n];
				 
		// b.id are unique in box
		obMRange(_m,_n) {
			if(o->id == wolf->x[_m].id) {
				ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("createChemin: Torder[%i].id=%i found twice",_n,o->id)));
			}
		}
	
		// successive orders match
		if(_n != 0 ) {
			if(!follow_orders(&wolf->x[_n-1],o)) {
				ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("createChemin: no match Torder[%i] -> Torder[%i] ",_n-1,_n)));		
			}
		}
	}
	#endif
	
	return;	
}

/*******************************************************************************
 (1/prodOmega) is shared between owners, and then shared between nodes of each owner.
 for each owner, it is:
 	 pow(gain_owner,nb_owners) = 1/prodOmega
 if the owner j owns _occ[j] nodes, it's gain_node for each node is:
 	pow(gain_node[j],_occ[j]) = gain_owner
 	
 input-output
 In:
 	prodOmega,
 	nbOwn,
 	nbNoeud,
 	ownOcc[j] for j in [O,nbOwn[
 Out:
 	omegaCorrige[i] for i in [0,nbNoeud[

	
the result omegaCorrige is such as the product(omegaCorrige[.])== 1 
 *******************************************************************************/

static void _calGains(TresChemin *chem) {
	short _i, _occ;
	short *occOwn = chem->occOwn;
	short *ownIndex = chem->ownIndex;
	double *omega = chem->omega;
	double *omegaCorrige = chem->omegaCorrige;
	short _dim = chem->wolf->dim;

	// the gain is shared between owners
	chem->gain = pow(chem->prodOmega, 1.0 /((double) chem->nbOwn));

	// it is shared between nodes
	obMRange(_i,_dim) {
		_occ = occOwn[ownIndex[_i]];
		omegaCorrige[_i] = omega[_i];
		if (_occ == 1)
			omegaCorrige[_i] /= chem->gain;
		else if (_occ > 1)
			omegaCorrige[_i] /= pow(chem->gain, 1.0 / ((double) _occ));
		else /* _occ is never zero */
			ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			 errmsg("_calGains: nbOwn <=0 ")));
		
	}

	return;
}


/*******************************************************************************
Computes the maximum flow fluxExact of pchemin.
 
This flow fluxExact[.] is such than:
 	fluxExact[i] = omegaCorrige[i] * fluxExact[i-1]
and such as quantities of stock can provide this flow
 
Input-output
************	
 In	pchemin, for i in [0,dim[ 
		->omegaCorrige[i]
		->box[j].qtt

 Out	fluxExact[.] the maximum flow, of pchemin->lon elts

returns the index i of the exhausted node.
		i in [0, dim[
Details
*******

_piom[i] is the product of omegas between the start of the path and a given node i.
	 For i==0 it is 1.
	 For i!=0 it is omegaCorrige[1]*.. omegaCorrige[i]

	 If we know the flow f[0] at the first node, the flow vector is given by:
		f[i] = f[0] * _piom[i]

If q[j] is the quantity of a stock j; Sj the set of nodes using this stock j,
and  f[i] is the flow of a node i,
Then we must have:  
	q[j] >= SUM(f[i]) for i in Sj
since:
	_piom[i] = f[i]/f[0]
we have:
	f[0] = MIN( q[j]/(SUM(_piom[i]) for i in Sj)) for j in stocks
and:
	f[i] = f[0] * _piom[i]
	
 *******************************************************************************/
static short _wolfMaximum(TresChemin *chem) {
	short 	_is, _jn; //,_jm;
	double	*_piom = chem->piom; //  _fPiomega(i)
	double  *omegaCorrige = chem->omegaCorrige;
	double  *fluxExact = chem->fluxExact;
	double	_min = -1.0, _cour;
	short	_iExhausted;	
	Twolf	*wolf = chem->wolf;
	short 	_dim = wolf->dim;
		
	// piom are obtained on each node, 
	/***********************************************************************/
	obMRange(_jn,_dim) {// loop on nodes
		short _k;
		
		/* computation of _piom */
		_piom[_jn] = 1.0;
		if(_jn > 0 ) 
			obMRange(_k,_jn) 
				_piom[_jn] *= omegaCorrige[_k + 1];
	}
	
	// minimum flow for the first node f[0]
	/***********************************************************************/
	// now _is is an index on nodes
	_iExhausted = 0;
	obMRange(_is,_dim) { // loop on nodes
		_cour = ((double) (wolf->x[_is].qtt)) /_piom[_is]; 
		if ((_is == 0) || (_cour < _min)) {
			_min = _cour;
			_iExhausted = _is;
		}
	} /* _iExhausted in [0,_dim-1] 
	*/
	
	// propagation to other nodes
	/***********************************************************************/
	obMRange(_jn,_dim)
		fluxExact[_jn] = _min * _piom[_jn];
		
	return _iExhausted;
}

/******************************************************************************
reduce the number of iteration of _rounding
the matrix of bits is at most _NMATBITS long 
******************************************************************************/
#define _NMATBITS (8)
#define _get_MIN(a,b) ((a<b)?a:b)
#define _get_bit_mat(mat,i) (mat &( 1 << (i & (_NMATBITS -1))))
/*******************************************************************************
floor,flow and mat are vectors of dimension _dim
floor and flow contains integers, and mat bits.
	in: dim,mat,floor
	out:flow
for each i, if the bit i of mat is 0, flow[i] := floor[i]
else flow[i] := floor[i]+1
*******************************************************************************/ 
#define _obtain_vertex(dim,mat,floor,flow) \
do { \
	short __j; \
	for(__j=0;__j<dim;__j++) { \
		flow[__j] = floor[__j]; \
		if (_get_bit_mat(mat,__j)) \
			flow[__j] += 1; \
	} \
} while(0)

/*******************************************************************************
 Computes a distance between two vectors vecExact and vecArrondi. 
 It is the angle alpha made by these vectors.
 We have:
 cos(alpha) = (vecExact.vecArrondi)/ (||vecArrondi||*||vecExact||)

 To minimize this distance between vecExact and vecArrondi, we maximize this cos(alpha).
 Since vecExact does not change between comparisons, alpha is minimized when the value:
	cos(alpha)*||vecExact|| is maximized,
 
 or (vecExact.vecArrondi)/ (||vecArrondi||) maximized.
 This value is returned by the function.
*******************************************************************************/
static double _idistance(short lon, 
				const double *vecExact,
				const int64 *vecArrondi) {
	double _s, _na, _va;
	short _i;

	_s = 0.0;
	_na = 0.0;
	obMRange(_i,lon) {
		_va = (double) (vecArrondi[_i]);
		_s += _va * vecExact[_i];
		_na += _va * _va;
	}
	return (_s / sqrt(_na));
}
/*******************************************************************************
 flow rounding

 When it can be found, gives the vector pchemin->fluxArrondi of integers
 the nearest of fluxExact, not greater than orders.qtt.

 box->dim short => must be <= 31 (2^dim loops)

 returns
 	 1 if a solution is found
  	 0 if no solution
 The solution is returned in pchemin->no[.].fluxArrondi

 when pchemin->cflags & obCLastIgnore,
 the last node does not limit the flow

 *******************************************************************************/
// #define WHY_REJECTED
static Tstatusflow _rounding(short iExhausted, TresChemin *chem) {
	short 	_i,  _k;
	short 	_matcour, _matmax, _matbest;
	bool 	_found;
	int64	*_flowNodes = chem->flowNodes;
	int64	*_floor = chem->floor;
	double 	_distcour, _distbest;
	Twolf 	*wolf = chem->wolf;
	short 	_dim = wolf->dim;
	double  *fluxExact = chem->fluxExact;
	double  *omega = chem->omega;
	short 	_ldim = (chem->lastignore)?(_dim-1):_dim;
	Tstatusflow _ret;

	// computes floor[] from fluxExact[]
	/***********************************************************************/
	obMRange(_i,_dim) {
		if(_i == iExhausted) { // will not change
			_floor[_i] = wolf->x[_i].qtt;
			continue; 
		} else {
			double _d = floor(fluxExact[_i]);
			int64 _f = (int64) _d;
			
			// sanity check
			if(_f < 0) _f = 0; 
			if(((double)(_f)) != _d) {
				ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("in _rounding, fluxExact[%i] = %f cannot be rounded",_i,fluxExact[_i])));
			}
			
			_floor[_i] = _f;
			//elog(WARNING,"flowc_maximum: _floor[%i]=%lli",_i,_f);
		}
	}

	//_matmax = 1 << _dim; // one bit for each node 
	_matmax = 1 << _get_MIN(_dim,_NMATBITS);
	
	// sanity check
	if(_matmax < 1) {
		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			 errmsg("in _rounding, matmax=%i,flow too long",_matmax)));
	}
	
	// for each vertex of the hypercude
	/***********************************************************************/
	_found = false;
	_ret = undefined;
	
	// at most _NMATBIT^2 iterations
	for (_matcour = 0; _matcour < _matmax; _matcour++) {
	
		//if((_matcour >> iExhausted) & 1)
		if(_get_bit_mat(_matcour,iExhausted))
			goto _continue;
		 
		// obtain the vertex _flowNodes
		/***************************************************************/
		_obtain_vertex(_dim,_matcour,_floor,_flowNodes);
		/* if _i == iExhausted, we have _flowNodes[_i] == box->x[_i].qtt
		else _flowNodes[_i] == _floor[i] (+1)  with _floor[i] <= box->x[i].qtt
		*/
		// several checkings
		/***************************************************************/
		
		// verify that flow <= box->k[.].qtt
		obMRange (_k,_dim) {
		
			// verify that order >= flow
			if(wolf->x[_k].qtt < _flowNodes[_k]) {
				#ifdef WHY_REJECTED 
					elog(WARNING,"_rounding 1: NOT order >= flow %s",flowc_vecIntStr(_dim,_flowNodes));
				#endif
				goto _continue;
			}
			
			// verify that flow > 0
			if(_flowNodes[_k] <= 0) {
				#ifdef WHY_REJECTED
					elog(WARNING,"_rounding 2: NOT flow>0 %s",flowc_vecIntStr(_dim,_flowNodes));
				#endif
				goto _continue;
			}
		}
				
		/* At this point, 
			* each node can provide flowNodes[],
			* all flowNodes[.] > 0
				=> every order provide something
				=> every owner provide something
			* the cycle exhausts the box
			* Omega ~= 1.0 
		*/
		
		{ 
			short _kp = _dim-1;
			double _precision = 1.E-8;
			
			/* the test is not made on the last node when lastignore */
			obMRange(_k,_ldim) {
				if(omega[_k] + _precision < ((double) _flowNodes[_k]) / ((double) _flowNodes[_kp])) {
					#ifdef WHY_REJECTED
						elog(WARNING,"flowc_maximum 4: NOT omega %f>=%f %s",omega[_k],((double) _flowNodes[_k]) / ((double) _flowNodes[_kp]),flowc_vecIntStr(_dim,_flowNodes));
					#endif
					_ret = refused;
					goto _continue;
				}
				_kp = _k;
			}
		} // order limits are observed
		

		// choose the best
		/***************************************************************/

		_distcour = _idistance(_dim, fluxExact, _flowNodes);
		// elog(WARNING,"flowc_maximum: matcour=%x _newdist=%f fluxExact=%s flowNodes=%s",_matcour,_distcour,_vecDoubleStr(_dim,fluxExact),_vecIntStr(_dim,_flowNodes));


		// this vertex is better than other found (if any)
		/***************************************************************/
		if( (!_found) || _distbest < _distcour) {
			// _distbest=max(cos(alpha)) === min(alpha) 
			_found = true;
			_distbest = _distcour;
			_matbest = _matcour;

		}
_continue:
		; // choose an other vertex
	};
	if(_found) {
		_obtain_vertex(_dim,_matbest,_floor,_flowNodes);
		//elog(WARNING,"flowc_maximum: _matbest=%x",_matbest);
		
		obMRange (_k,_dim) {
			wolf->x[_k].flowr = _flowNodes[_k];	
		}
		
		_ret = draft; 
	} // else _ret is undefined or refused
	return _ret; 
}

/******************************************************************************
 * 
 *****************************************************************************/
char * wolfc_cheminToStr(TresChemin *chem) {
	StringInfoData buf;
	Twolf *wolf = chem->wolf;
	short _dim = wolf->dim;
	short _n,_o;
	
	initStringInfo(&buf);
	appendStringInfo(&buf, "\n%s\n",ywolf_allToStr(wolf));
	if(chem->status == refused) {	
		appendStringInfo(&buf, "CHEMIN refused prodOmega=%f ",chem->prodOmega);
		appendStringInfo(&buf, "\nno[.].omega=[");
		obMRange(_n,_dim) {
			appendStringInfo(&buf, "%f, ", chem->omega[_n]);
		}
		appendStringInfo(&buf, "]\n");
		
	} else if(chem->status == draft || chem->status == undefined) {
	
		appendStringInfo(&buf, "CHEMIN  %s lastignore=%c nbOwn=%i gain=%f prodOmega=%f ", 
			ywolf_statusToStr(chem->status),
			(chem->lastignore)?'t':'f',chem->nbOwn,chem->gain,chem->prodOmega);
	
		appendStringInfo(&buf, "\noccOwn[.]=[");
		_o = chem->nbOwn;
		obMRange(_n,_o) {
			appendStringInfo(&buf, "%i, ", chem->occOwn[_n]);
		}
	
		appendStringInfo(&buf, "]\nownIndex[.]=[");
		obMRange(_n,_dim) {
			appendStringInfo(&buf, "%i, ", chem->ownIndex[_n]);
		}	
			
		appendStringInfo(&buf, "]\nomega.[.]=[");
		obMRange(_n,_dim) {
			appendStringInfo(&buf, "%f, ", chem->omega[_n]);
		}
	
		appendStringInfo(&buf, "]\npiom[.]=[");
		obMRange(_n,_dim) {
			appendStringInfo(&buf, "%f, ", chem->piom[_n]);
		}		
	
		appendStringInfo(&buf, "]\nfluxExact[.]=[");
		obMRange(_n,_dim) {
			appendStringInfo(&buf, "%f, ", chem->fluxExact[_n]);
		}
	
		appendStringInfo(&buf, "]\nomegaCorrige[.]=[");
		obMRange(_n,_dim) {
			appendStringInfo(&buf, "%f, ", chem->omegaCorrige[_n]);
		}	
	
		appendStringInfo(&buf, "]\nflowNodes[.]=[");
		obMRange(_n,_dim) {
			appendStringInfo(&buf, INT64_FORMAT ", ", chem->flowNodes[_n]);
		}	
		appendStringInfo(&buf, "]\n");
	} else  {
	
		appendStringInfo(&buf, "CHEMIN  %s lastignore=%c nbOwn=%i gain=%f prodOmega=%f ", 
			ywolf_statusToStr(chem->status),
			(chem->lastignore)?'t':'f',chem->nbOwn,chem->gain,chem->prodOmega);
	}

	return buf.data;
}
/******************************************************************************
 * 
 *****************************************************************************/
char * wolfc_vecIntStr(short dim,int64 *vecInt) {
	StringInfoData buf;
	short _n;
	
	initStringInfo(&buf);
	appendStringInfo(&buf, "[");
	obMRange(_n,dim) {
		appendStringInfo(&buf,  INT64_FORMAT ", ", vecInt[_n]);
	}	
	appendStringInfo(&buf, "]");

	return buf.data;
}
/******************************************************************************
 * 
 *****************************************************************************/
char * wolfc_vecDoubleStr(short dim,double *vecDouble) {
	StringInfoData buf;
	short _n;
	
	initStringInfo(&buf);
	appendStringInfo(&buf, "[");
	obMRange(_n,dim) {
		appendStringInfo(&buf, "%f, ", vecDouble[_n]);
	}	
	appendStringInfo(&buf, "]");

	return buf.data;
}

