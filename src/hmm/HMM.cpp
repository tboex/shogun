// HMM.cpp: implementation of the CHMM class.
// $Id$
//////////////////////////////////////////////////////////////////////

#include <math.h>
#include "hmm/HMM.h"
#include "lib/Mathmatics.h"
#include "lib/io.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

#define ARRAY_SIZE 65336

#ifdef SUNOS
extern "C" int	finite(double);
#endif

#ifdef PARALLEL 
 #include <unistd.h>
 #include <pthread.h>
#ifdef SUNOS
 #include <thread.h>
#endif
 int NUM_PARALLEL= sysconf( _SC_NPROCESSORS_ONLN );
#else
 int NUM_PARALLEL=1 ;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define GOTN (1<<1)
#define GOTM (1<<2)
#define GOTO (1<<3)
#define GOTa (1<<4)
#define GOTb (1<<5)
#define GOTp (1<<6)
#define GOTq (1<<7)

#define GOTlearn_a (1<<1)
#define GOTlearn_b (1<<2)
#define GOTlearn_p (1<<3)
#define GOTlearn_q (1<<4)
#define GOTconst_a (1<<5)
#define GOTconst_b (1<<6)
#define GOTconst_p (1<<7)
#define GOTconst_q (1<<8)

enum E_STATE
{
	INITIAL,
	ARRAYs,
	GET_N,
	GET_M,
	GET_ORDER,
	GET_a,
	GET_b,
	GET_p,
	GET_q,
	GET_learn_a,
	GET_learn_b,
	GET_learn_p,
	GET_learn_q,
	GET_const_a,
	GET_const_b,
	GET_const_p,
	GET_const_q,
	COMMENT,
	END
};

double* CHMM::feature_cache_sv=NULL;	
double* CHMM::feature_cache_obs=NULL;	
int CHMM::num_features=0;	
bool CHMM::feature_cache_in_question=false;	
unsigned int CHMM::feature_cache_checksums[8];	
unsigned int CHMM::features_crc32[32];

#ifdef FIX_POS
const char CHMM::CModel::FIX_DISALLOWED=0 ;
const char CHMM::CModel::FIX_ALLOWED=1 ;
const char CHMM::CModel::FIX_DEFAULT=-1 ;
const REAL CHMM::CModel::DISALLOWED_PENALTY=CMath::ALMOST_NEG_INFTY ;
#endif

CHMM::CModel::CModel()
{
	const_a=new int[ARRAY_SIZE];				///////static fixme 
	const_b=new int[ARRAY_SIZE];
	const_p=new int[ARRAY_SIZE];
	const_q=new int[ARRAY_SIZE];
	const_a_val=new REAL[ARRAY_SIZE];			///////static fixme 
	const_b_val=new REAL[ARRAY_SIZE];
	const_p_val=new REAL[ARRAY_SIZE];
	const_q_val=new REAL[ARRAY_SIZE];


	learn_a=new int[ARRAY_SIZE];
	learn_b=new int[ARRAY_SIZE];
	learn_p=new int[ARRAY_SIZE];
	learn_q=new int[ARRAY_SIZE];

#ifdef FIX_POS
	fix_pos_state = new char[ARRAY_SIZE];
#endif
	for (int i=0; i<ARRAY_SIZE; i++)
	  {
	    const_a[i]=-1 ;
	    const_b[i]=-1 ;
	    const_p[i]=-1 ;
	    const_q[i]=-1 ;
	    const_a_val[i]=1.0 ;
	    const_b_val[i]=1.0 ;
	    const_p_val[i]=1.0 ;
	    const_q_val[i]=1.0 ;
	    learn_a[i]=-1 ;
	    learn_b[i]=-1 ;
	    learn_p[i]=-1 ;
	    learn_q[i]=-1 ;
#ifdef FIX_POS
	    fix_pos_state[i] = FIX_DEFAULT ;
#endif
	  } ;
}

CHMM::CModel::~CModel()
{
	delete[] const_a;
	delete[] const_b;
	delete[] const_p;
	delete[] const_q;
	delete[] const_a_val;
	delete[] const_b_val;
	delete[] const_p_val;
	delete[] const_q_val;

	delete[] learn_a;
	delete[] learn_b;
	delete[] learn_p;
	delete[] learn_q;

#ifdef FIX_POS
	delete[] fix_pos_state;
#endif
}

CHMM::CHMM(int N, int M, int ORDER, CModel* model, REAL PSEUDO)
{
  status=initialize(N, M, ORDER, model, PSEUDO);
}

CHMM::CHMM(FILE* model_file, REAL PSEUDO)
{
  status=initialize(0, 0, 1, NULL, PSEUDO, model_file);
}

CHMM::~CHMM()
{
    delete model ;
    invalidate_top_feature_cache(INVALID);
    free_state_dependend_arrays();
	
	if (!reused_caches)
	{
#ifdef PARALLEL
		for (int i=0; i<NUM_PARALLEL; i++)
		{
			delete[] alpha_cache[i].table;
			delete[] beta_cache[i].table;
			alpha_cache[i].table=NULL;
			beta_cache[i].table=NULL;
		}

		delete[] alpha_cache;
		delete[] beta_cache;
		alpha_cache=NULL;
		beta_cache=NULL;
#else
		delete[] alpha_cache.table;
		delete[] beta_cache.table;
		alpha_cache.table=NULL;
		beta_cache.table=NULL;
#endif

#ifndef NOVIT
		delete[] states_per_observation_psi;
		states_per_observation_psi=NULL;
#endif // NOVIT
	}

#ifdef LOG_SUM_ARRAY
#ifdef PARALLEL
    {
	for (int i=0; i<NUM_PARALLEL; i++)
	    delete[] arrayS[i];
	delete[] arrayS ;
    } ;
#else //PARALLEL
    delete[] arrayS;
#endif //PARALLEL
#endif //LOG_SUM_ARRAY

    if (!reused_caches)
    {
#ifndef NOVIT
#ifdef PARALLEL
	delete[] path_prob_updated ;
	delete[] path_prob_dimension ;
	for (int i=0; i<NUM_PARALLEL; i++)
	    delete[] path[i] ;
#endif
	delete[] path;
#endif
    }
}
  
bool CHMM::alloc_state_dependend_arrays()
{

	if (!transition_matrix_a && !observation_matrix_b && !initial_state_distribution_p && !end_state_distribution_q)
	{
		transition_matrix_a=new REAL[N*N];
		observation_matrix_b=new REAL[N*M];	
		initial_state_distribution_p=new REAL[N];
		end_state_distribution_q=new REAL[N];
		init_model_random();
		convert_to_log();
	}

#ifdef PARALLEL
	for (int i=0; i<NUM_PARALLEL; i++)
	{
		arrayN1[i]=new REAL[N];
		arrayN2[i]=new REAL[N];
	}
#else //PARALLEL
	arrayN1=new REAL[N];
	arrayN2=new REAL[N];
#endif //PARALLEL

#ifdef LOG_SUMARRAY
#ifdef PARALLEL
	for (int i=0; i<NUM_PARALLEL; i++)
		arrayS[i]=new REAL[(int)(this->N/2+1)];
#else //PARALLEL
	arrayS=new REAL[(int)(this->N/2+1)];
#endif //PARALLEL
#endif //LOG_SUMARRAY
	transition_matrix_A=new REAL[this->N*this->N];
	observation_matrix_B=new REAL[this->N*this->M];
	
	if (p_observations)
	{
#ifdef PARALLEL
	    if (alpha_cache[0].table!=NULL)
#else
		if (alpha_cache.table!=NULL)
#endif
		    set_observations(p_observations);
		else
		    set_observation_nocache(p_observations);
	}
	else
	{

	    CIO::message("setting observations\n");
	    set_observations(p_observations);
	}
	
	this->invalidate_model();

	return ((transition_matrix_A != NULL) && (observation_matrix_B != NULL) && 
			(transition_matrix_a != NULL) && (observation_matrix_b != NULL) && (initial_state_distribution_p != NULL) &&
			(end_state_distribution_q != NULL));
}

void CHMM::free_state_dependend_arrays()
{
#ifdef PARALLEL
	for (int i=0; i<NUM_PARALLEL; i++)
	{
		delete[] arrayN1[i];
		delete[] arrayN2[i];

		arrayN1[i]=NULL;
		arrayN2[i]=NULL;
	}
#else
	delete[] arrayN1;
	delete[] arrayN2;
	arrayN1=NULL;
	arrayN2=NULL;
#endif

	delete[] transition_matrix_A;
	delete[] observation_matrix_B;
	delete[] transition_matrix_a;
	delete[] observation_matrix_b;
	delete[] initial_state_distribution_p;
	delete[] end_state_distribution_q;

	transition_matrix_A=NULL;
	observation_matrix_B=NULL;
	transition_matrix_a=NULL;
	observation_matrix_b=NULL;
	initial_state_distribution_p=NULL;
	end_state_distribution_q=NULL;


}

bool CHMM::initialize(int N, int M, int ORDER_, CModel* model, 
					  REAL PSEUDO, FILE* modelfile)
{
	//yes optimistic
	bool files_ok=true;

	if  (M!=0)	
	{
		if (N>(1<<(8*sizeof(T_STATES))))
			CIO::message(stderr, "########\nNUMBER OF STATES TOO LARGE. Maximum is %i\n########\n", (1<<(8*sizeof(T_STATES)))) ;

		MAX_M=(BYTE)ceil(log(M)/log(2));

		if (M>(1<<(8*sizeof(T_OBSERVATIONS))))
			CIO::message(stderr, "########\nALPHABET TOO LARGE. Maximum is %i\n########\n", (int)((1<<(8*sizeof(T_OBSERVATIONS)))));

		if ( (unsigned)MAX_M*ORDER_> (unsigned)8*sizeof(T_OBSERVATIONS) )
			CIO::message(stderr, "########\nORDER TOO LARGE. Maximum is %i\n########\n", (int)(((REAL)8*sizeof(T_OBSERVATIONS))/((REAL)MAX_M))) ;

		//map higher order to alphabet
		M=	M << (MAX_M * (ORDER_-1));
	}
	else
		MAX_M=0;

	//initialize parameters
	this->M= M;
	this->N= N;
	this->transition_matrix_a=NULL;
	this->observation_matrix_b=NULL;
	this->initial_state_distribution_p=NULL;
	this->end_state_distribution_q=NULL;
	this->ORDER=ORDER_;
	this->PSEUDO= PSEUDO;
	this->model= model;
	this->p_observations=NULL;
	this->reused_caches=false;

#ifdef PARALLEL
	alpha_cache=new T_ALPHA_BETA[NUM_PARALLEL] ;
	beta_cache=new T_ALPHA_BETA[NUM_PARALLEL] ;
	states_per_observation_psi=new P_STATES[NUM_PARALLEL] ;

	for (int i=0; i<NUM_PARALLEL; i++)
	{
		this->alpha_cache[i].table=NULL;
		this->beta_cache[i].table=NULL;
		this->alpha_cache[i].dimension=0;
		this->beta_cache[i].dimension=0;
#ifndef NOVIT
		this->states_per_observation_psi[i]=NULL ;
#endif // NOVIT
	}

#else // PARALLEL
	this->alpha_cache.table=NULL;
	this->beta_cache.table=NULL;
	this->alpha_cache.dimension=0;
	this->beta_cache.dimension=0;
#ifndef NOVIT
	this->states_per_observation_psi=NULL ;
#endif //NOVIT

#endif //PARALLEL

	
	if (modelfile)
		files_ok= files_ok && load_model(modelfile);
	  
#ifdef PARALLEL
	  path_prob_updated=new bool[NUM_PARALLEL];
	  path_prob_dimension=new int[NUM_PARALLEL];

	  path=new P_STATES[NUM_PARALLEL];

	  for (i=0; i<NUM_PARALLEL; i++)
	  {
#ifndef NOVIT
	      this->path[i]=NULL;
#endif // NOVIT
	  }
#else // PARALLEL
#ifndef NOVIT
	this->path=NULL;
#endif //NOVIT

#endif //PARALLEL
	this->loglikelihood=false;
	this->invalidate_model();

#ifdef PARALLEL
	arrayN1=new P_REAL[NUM_PARALLEL];
	arrayN2=new P_REAL[NUM_PARALLEL];
#endif //PARALLEL

#ifdef LOG_SUMARRAY
#ifdef PARALLEL
	arrayS=new P_REAL[NUM_PARALLEL] ;	  
#endif //PARALLEL
#endif //LOG_SUMARRAY

	alloc_state_dependend_arrays();
	
	return	((files_ok) &&
			(transition_matrix_A != NULL) && (observation_matrix_B != NULL) && 
			(transition_matrix_a != NULL) && (observation_matrix_b != NULL) && (initial_state_distribution_p != NULL) &&
			(end_state_distribution_q != NULL));
}

//------------------------------------------------------------------------------------//

//forward algorithm
//calculates Pr[O_0,O_1, ..., O_t, q_time=S_i| lambda] for 0<= time <= T-1
//Pr[O|lambda] for time > T
REAL CHMM::forward_comp(int time, int state, int dimension)
{
	T_ALPHA_BETA_TABLE* alpha_new;
	T_ALPHA_BETA_TABLE* alpha;
	T_ALPHA_BETA_TABLE* dummy;
	if (time<1)
		time=0;

	int wanted_time=time;

	if (ALPHA_CACHE(dimension).table)
	  {
	    alpha=&ALPHA_CACHE(dimension).table[0];
	    alpha_new=&ALPHA_CACHE(dimension).table[N];
	    time=p_observations->get_obs_T(dimension)+1;
	  }
	else
	  {
	    alpha_new=(T_ALPHA_BETA_TABLE*)ARRAYN1(dimension);
	    alpha=(T_ALPHA_BETA_TABLE*)ARRAYN2(dimension);
	  }
	
	if (time<1)
	  return get_p(state) + get_b(state, p_observations->get_obs(dimension,0));
	else
	  {
	    //initialization	alpha_1(i)=p_i*b_i(O_1)
	    for (register int i=0; i<N; i++)
	      alpha[i] = get_p(i) + get_b(i, p_observations->get_obs(dimension,0)) ;
	    
	    //induction		alpha_t+1(j) = (sum_i=1^N alpha_t(i)a_ij) b_j(O_t+1)
	    for (register int t=1; t<time && t < p_observations->get_obs_T(dimension); t++)
	      {
		
		for (register int j=0; j<N; j++)
		  {
		    register int i ;
#ifdef LOG_SUM_ARRAY
		    for (i=0; i<(N>>1); i++)
		      ARRAYS(dimension)[i]=math.logarithmic_sum(alpha[i<<1] + get_a(i<<1,j),
						     alpha[(i<<1)+1] + get_a((i<<1)+1,j));
		    if (N%2==1) 
		      alpha_new[j]=get_b(j, p_observations->get_obs(dimension,t))+
			math.logarithmic_sum(alpha[N-1]+get_a(N-1,j), 
					     math.logarithmic_sum_array(ARRAYS(dimension), N>>1)) ;
		    else
		      alpha_new[j]=get_b(j, p_observations->get_obs(dimension,t))+math.logarithmic_sum_array(ARRAYS(dimension), N>>1) ;
#else //LOG_SUM_ARRAY
		    REAL sum=-math.INFTY;  
		    for (i=0; i<N; i++)
		      sum= math.logarithmic_sum(sum, alpha[i] + get_a(i,j));
		    
		    alpha_new[j]= sum + get_b(j, p_observations->get_obs(dimension,t));
#endif //LOG_SUM_ARRAY
		  }
		
		if (!ALPHA_CACHE(dimension).table)
		  {
		    dummy=alpha;
		    alpha=alpha_new;
		    alpha_new=dummy;	//switch alpha/alpha_new
		  }
		else
		  {
		    alpha=alpha_new;
		    alpha_new+=N;		//perversely pointer arithmetic
		  }
	      }
	    

	    if (time<p_observations->get_obs_T(dimension))
	      {
		register int i;
#ifdef LOG_SUM_ARRAY
		for (i=0; i<(N>>1); i++)
		  ARRAYS(dimension)[i]=math.logarithmic_sum(alpha[i<<1] + get_a(i<<1,state),
						 alpha[(i<<1)+1] + get_a((i<<1)+1,state));
		if (N%2==1) 
		  return get_b(state, p_observations->get_obs(dimension,time))+
		    math.logarithmic_sum(alpha[N-1]+get_a(N-1,state), 
					 math.logarithmic_sum_array(ARRAYS(dimension), N>>1)) ;
		else
		  return get_b(state, p_observations->get_obs(dimension,time))+math.logarithmic_sum_array(ARRAYS(dimension), N>>1) ;
#else //LOG_SUM_ARRAY
		register REAL sum=-math.INFTY; 
		for (i=0; i<N; i++)
		  sum= math.logarithmic_sum(sum, alpha[i] + get_a(i, state));
		
		return sum + get_b(state, p_observations->get_obs(dimension,time));
#endif //LOG_SUM_ARRAY
	      }
	    else
	      {
		// termination
		register int i ; 
		REAL sum ; 
#ifdef LOG_SUM_ARRAY
		for (i=0; i<(N>>1); i++)
		  ARRAYS(dimension)[i]=math.logarithmic_sum(alpha[i<<1] + get_q(i<<1),
						 alpha[(i<<1)+1] + get_q((i<<1)+1));
		if (N%2==1) 
		  sum=math.logarithmic_sum(alpha[N-1]+get_q(N-1), 
					   math.logarithmic_sum_array(ARRAYS(dimension), N>>1)) ;
		else
		  sum=math.logarithmic_sum_array(ARRAYS(dimension), N>>1) ;
#else //LOG_SUM_ARRAY
		sum=-math.INFTY; 
		for (i=0; i<N; i++)		 	                      //sum over all paths
		  sum=math.logarithmic_sum(sum, alpha[i] + get_q(i));     //to get model probability
#endif //LOG_SUM_ARRAY
		
		if (!ALPHA_CACHE(dimension).table)
		  return sum;
		else
		  {
		    /*printf("setting alpha_cache.dim=%i\n", dimension) ;*/
		    ALPHA_CACHE(dimension).dimension=dimension;
		    ALPHA_CACHE(dimension).updated=true;
		    ALPHA_CACHE(dimension).sum=sum;
		    
		    if (wanted_time<p_observations->get_obs_T(dimension))
		      return ALPHA_CACHE(dimension).table[wanted_time*N+state];
		    else
		      return ALPHA_CACHE(dimension).sum;
		  }
	      }
	  }
}


//backward algorithm
//calculates Pr[O_t+1,O_t+2, ..., O_T| q_time=S_i, lambda] for 0<= time <= T-1
//Pr[O|lambda] for time >= T
REAL CHMM::backward_comp(int time, int state, int dimension)
{
	T_ALPHA_BETA_TABLE* beta_new;
	T_ALPHA_BETA_TABLE* beta;
	T_ALPHA_BETA_TABLE* dummy;
	int wanted_time=time;

	if (time<0)
		forward(time, state, dimension);
		
	if (BETA_CACHE(dimension).table)
	  {
	    beta=&BETA_CACHE(dimension).table[N*(p_observations->get_obs_T(dimension)-1)];
	    beta_new=&BETA_CACHE(dimension).table[N*(p_observations->get_obs_T(dimension)-2)];
	    time=-1;
	  }
	else
	  {
	    beta_new=(T_ALPHA_BETA_TABLE*)ARRAYN1(dimension);
	    beta=(T_ALPHA_BETA_TABLE*)ARRAYN2(dimension);
	  }
	
	if (time>=p_observations->get_obs_T(dimension)-1)
//	  return 0;
//	else if (time==p_observations->get_obs_T(dimension)-1)
	  return get_q(state);
	else
	  {
				//initialization	beta_T(i)=q(i)
	    for (register int i=0; i<N; i++)
	      beta[i]=get_q(i);
	    
				//induction		beta_t(i) = (sum_j=1^N a_ij*b_j(O_t+1)*beta_t+1(j)
	    for (register int t=p_observations->get_obs_T(dimension)-1; t>time+1 && t>0; t--)
	      {
		for (register int i=0; i<N; i++)
		  {
		    register int j ;
#ifdef LOG_SUM_ARRAY
		    for (j=0; j<(N>>1); j++)
		      ARRAYS(dimension)[j]=math.logarithmic_sum(
						     get_a(i, j<<1) + get_b(j<<1, p_observations->get_obs(dimension,t)) + beta[j<<1],
						     get_a(i, (j<<1)+1) + get_b((j<<1)+1, p_observations->get_obs(dimension,t)) + beta[(j<<1)+1]);
		    if (N%2==1) 
		      beta_new[i]=math.logarithmic_sum(get_a(i, N-1) + get_b(N-1, p_observations->get_obs(dimension,t)) + beta[N-1], 
						       math.logarithmic_sum_array(ARRAYS(dimension), N>>1)) ;
		    else
		      beta_new[i]=math.logarithmic_sum_array(ARRAYS(dimension), N>>1) ;
#else //LOG_SUM_ARRAY				
		    REAL sum=-math.INFTY; 
		    for (j=0; j<N; j++)
		      sum= math.logarithmic_sum(sum, get_a(i, j) + get_b(j, p_observations->get_obs(dimension,t)) + beta[j]);

		    beta_new[i]=sum;
		    //printf("beta[%d][%d]=%f\n",t,i,(double)sum);
#endif //LOG_SUM_ARRAY
		  }
		
		if (!BETA_CACHE(dimension).table)
		  {
		    dummy=beta;
		    beta=beta_new;
		    beta_new=dummy;	//switch beta/beta_new
		  }
		else
		  {
		    beta=beta_new;
		    beta_new-=N;		//perversely pointer arithmetic
		  }
	      }
	    
	    if (time>=0)
	      {
		register int j ;
#ifdef LOG_SUM_ARRAY
		for (j=0; j<(N>>1); j++)
		  ARRAYS(dimension)[j]=math.logarithmic_sum(
						 get_a(state, j<<1) + get_b(j<<1, p_observations->get_obs(dimension,time+1)) + beta[j<<1],
						 get_a(state, (j<<1)+1) + get_b((j<<1)+1, p_observations->get_obs(dimension,time+1)) + beta[(j<<1)+1]);
		if (N%2==1) 
		  return math.logarithmic_sum(get_a(state, N-1) + get_b(N-1, p_observations->get_obs(dimension,time+1)) + beta[N-1], 
					      math.logarithmic_sum_array(ARRAYS(dimension), N>>1)) ;
		else
		  return math.logarithmic_sum_array(ARRAYS(dimension), N>>1) ;
#else //LOG_SUM_ARRAY
		REAL sum=-math.INFTY; 
		for (j=0; j<N; j++)
		  sum= math.logarithmic_sum(sum, get_a(state, j) + get_b(j, p_observations->get_obs(dimension,time+1))+beta[j]);
		
		return sum;
#endif //LOG_SUM_ARRAY
	      }
	    else // time<0
	      {
		if (BETA_CACHE(dimension).table)
		  {
#ifdef LOG_SUM_ARRAY//AAA
		    for (int j=0; j<(N>>1); j++)
		      ARRAYS(dimension)[j]=math.logarithmic_sum(get_p(j<<1) + get_b(j<<1, p_observations->get_obs(dimension,0))+beta[j<<1],
								get_p((j<<1)+1) + get_b((j<<1)+1, p_observations->get_obs(dimension,0))+beta[(j<<1)+1]) ;
		    if (N%2==1) 
		      BETA_CACHE(dimension).sum=math.logarithmic_sum(get_p(N-1) + get_b(N-1, p_observations->get_obs(dimension,0))+beta[N-1],
								     math.logarithmic_sum_array(ARRAYS(dimension), N>>1)) ;
		    else
		      BETA_CACHE(dimension).sum=math.logarithmic_sum_array(ARRAYS(dimension), N>>1) ;
#else //LOG_SUM_ARRAY
		    REAL sum=-math.INFTY; 
		    for (register int j=0; j<N; j++)
		      sum= math.logarithmic_sum(sum, get_p(j) + get_b(j, p_observations->get_obs(dimension,0))+beta[j]);
		    //printf("sum=%f,osum=%f,diff=%f\n",sum,BETA_CACHE(dimension).sum,sum-BETA_CACHE(dimension).sum) ;
		    BETA_CACHE(dimension).sum=sum;
#endif //LOG_SUM_ARRAY
		    BETA_CACHE(dimension).dimension=dimension;
		    BETA_CACHE(dimension).updated=true;
		    
		    if (wanted_time<p_observations->get_obs_T(dimension))
		      return BETA_CACHE(dimension).table[wanted_time*N+state];
		    else
		      return BETA_CACHE(dimension).sum;
		  }
		else
		{
		    REAL sum=-math.INFTY; // apply LOG_SUM_ARRAY -- no cache ... does not make very sense anyway...
		    for (register int j=0; j<N; j++)
		      sum= math.logarithmic_sum(sum, get_p(j) + get_b(j, p_observations->get_obs(dimension,0))+beta[j]);
			return sum;
		}
	  }
	}
}

#ifndef NOVIT
//calculates probability  of best path through the model lambda AND path itself
//using viterbi algorithm
REAL CHMM::best_path(int dimension)
{
        if (!p_observations)
	  return -1;

	if (dimension==-1) 
	  {
	    if (!all_path_prob_updated)
	      {
		printf("computing full viterbi likelihood\n") ;
		REAL sum = 0 ;
		for (int i=0; i<p_observations->get_DIMENSION(); i++)
		  sum+=best_path(i) ;
		sum /= p_observations->get_DIMENSION() ;
		all_pat_prob=sum ;
		all_path_prob_updated=true ;
		return sum ;
	      } else
		return all_pat_prob ;
	  } ;
	
	if (!STATES_PER_OBSERVATION_PSI(dimension))
	  return -1 ;

	if (!p_observations->get_obs_vector(dimension))
		return -1;

	if (PATH_PROB_UPDATED(dimension) && dimension==PATH_PROB_DIMENSION(dimension))
	  return pat_prob;
	else
	  {
	    register REAL* delta= ARRAYN2(dimension);
	    register REAL* delta_new= ARRAYN1(dimension);
	    	    
	    { //initialization
	      for (register int i=0; i<N; i++)
		{
		  delta[i]=get_p(i)+get_b(i, p_observations->get_obs(dimension,0));
		  set_psi(0, i, 0, dimension);
		}
	    } 

#ifdef PATHDEBUG
	    REAL worst=-CMath::INFTY/4 ;
#endif
	    //recursion
	    for (register int t=1; t<p_observations->get_obs_T(dimension); t++)
	      {
		register REAL* dummy;
		register int NN=N ;
		for (register int j=0; j<NN; j++)
		  {
		    register REAL * matrix_a=&transition_matrix_a[j*N] ; // sorry for that
		    register REAL maxj=delta[0] + matrix_a[0];
		    register int argmax=0;
		    
		    for (register int i=1; i<NN; i++)
		      {
			register REAL temp = delta[i] + matrix_a[i];
			
			if (temp>maxj)
			  {
			    maxj=temp;
			    argmax=i;
			  }
		      }
#ifdef FIX_POS
		    if ((!model) || (model->get_fix_pos_state(t,j,NN)!=CModel::FIX_DISALLOWED))
#endif
		      delta_new[j]=maxj + get_b(j,p_observations->get_obs(dimension,t));
#ifdef FIX_POS
		    else
		      delta_new[j]=maxj + get_b(j,p_observations->get_obs(dimension,t)) + CModel::DISALLOWED_PENALTY;
#endif		      
		    set_psi(t, j, argmax, dimension);
		  }
		
#ifdef PATHDEBUG
		REAL best=log(0) ;
		for (int jj=0; jj<N; jj++)
		  if (delta_new[jj]>best)
		    best=delta_new[jj] ;

		if (best<-CMath::INFTY/2)
		  {
		   CIO::message("worst case at %i: %e:%e\n", t, best, worst) ;
		    worst=best ;
		  } ;
#endif		

		dummy=delta;
		delta=delta_new;
		delta_new=dummy;	//switch delta/delta_new
	      }
	    
	    
	    { //termination
	      register REAL maxj=delta[0]+get_q(0);
	      register int argmax=0;
	      
	      for (register int i=1; i<N; i++)
		{
		  register REAL temp=delta[i]+get_q(i);
		  
		  if (temp>maxj)
		    {
		      maxj=temp;
		      argmax=i;
		    }
		}
	      pat_prob=maxj;
	      PATH(dimension)[p_observations->get_obs_T(dimension)-1]=argmax;
	    } ;
	    
	    
	    { //state sequence backtracking
	      for (register int t=p_observations->get_obs_T(dimension)-1; t>0; t--)
		{
		  PATH(dimension)[t-1]=get_psi(t, PATH(dimension)[t], dimension);
		}
	    }
	    PATH_PROB_UPDATED(dimension)=true;
	    PATH_PROB_DIMENSION(dimension)=dimension;
	    return pat_prob ;
	  }
}
#endif // NOVIT

#ifndef PARALLEL
REAL CHMM::model_probability_comp() 
{
  //for faster calculation cache model probability
 CIO::message("computing full model probablity\n") ;
  mod_prob=0 ;
  for (int dim=0; dim<p_observations->get_DIMENSION(); dim++)		//sum in log space
      mod_prob+=forward(p_observations->get_obs_T(dim), 0, dim);

  mod_prob_updated=true;
  return mod_prob;
}

#else

REAL CHMM::model_probability_comp() 
{
    pthread_t *threads=new pthread_t[NUM_PARALLEL];
    S_MODEL_PROB_PARAM *params=new S_MODEL_PROB_PARAM[NUM_PARALLEL];

    CIO::message("computing full model probablity\n");
    mod_prob=0;

    for (int cpu=0; cpu<NUM_PARALLEL; cpu++)
    {
	params[cpu].hmm=this ;
	params[cpu].dim_start= p_observations->get_DIMENSION()*cpu/NUM_PARALLEL;
	params[cpu].dim_stop= p_observations->get_DIMENSION()*(cpu+1)/NUM_PARALLEL;
#ifdef SUNOS
	thr_create(NULL,0,bw_dim_prefetch, (void*)&params[i], PTHREAD_SCOPE_SYSTEM, &threads[i]);
#else // SUNOS
	pthread_create(&threads[i], NULL, bw_dim_prefetch, (void*)&params[i]);
#endif // SUNOS
    }

    for (cpu=0; cpu<NUM_PARALLEL; cpu++)
    {
	void* ret;
	pthread_join(threads[i], &ret);
	mod_prob+=(REAL) ret;
    }

    delete[] threads;
    delete[] params;

    mod_prob_updated=true;
    return mod_prob;
}

void* CHMM::bw_dim_prefetch(void * params)
{
    CHMM* hmm=((S_THREAD_PARAM*)params)->hmm ;
    int dim=((S_THREAD_PARAM*)params)->dim ;
    REAL*p_buf=((S_THREAD_PARAM*)params)->p_buf ;
    REAL*q_buf=((S_THREAD_PARAM*)params)->q_buf ;
    REAL*a_buf=((S_THREAD_PARAM*)params)->a_buf ;
    REAL*b_buf=((S_THREAD_PARAM*)params)->b_buf ;
    ((S_THREAD_PARAM*)params)->ret = hmm->prefetch(dim, true, p_buf, q_buf, a_buf, b_buf) ;
    return NULL ;
}

REAL CHMM::model_prob_thread(void* params)
{
    CHMM* hmm=((S_THREAD_PARAM*)params)->hmm ;
    int dim_start=((S_THREAD_PARAM*)params)->dim_start;
    int dim_stop=((S_THREAD_PARAM*)params)->dim_stop;
    
    REAL prob=0;
    for (int dim=dim_start; dim<dim_stop; dim++)
	hmm->model_probability(dim);

    ab_buf_comp(p_buf, q_buf, a_buf, b_buf, dim) ;
    return modprob ;
} ;

void* CHMM::bw_dim_prefetch(void * params)
{
    CHMM* hmm=((S_THREAD_PARAM*)params)->hmm ;
    int dim=((S_THREAD_PARAM*)params)->dim ;
    REAL*p_buf=((S_THREAD_PARAM*)params)->p_buf ;
    REAL*q_buf=((S_THREAD_PARAM*)params)->q_buf ;
    REAL*a_buf=((S_THREAD_PARAM*)params)->a_buf ;
    REAL*b_buf=((S_THREAD_PARAM*)params)->b_buf ;
    ((S_THREAD_PARAM*)params)->ret = hmm->prefetch(dim, true, p_buf, q_buf, a_buf, b_buf) ;
    return NULL ;
}

#ifndef NOVIT
void* CHMM::vit_dim_prefetch(void * params)
{
    CHMM* hmm=((S_THREAD_PARAM*)params)->hmm ;
    int dim=((S_THREAD_PARAM*)params)->dim ;
    ((S_THREAD_PARAM*)params)->ret = hmm->prefetch(dim, false) ;
    return NULL ;
} ;
#endif // NOVIT

REAL CHMM::prefetch(int dim, bool bw, REAL* p_buf, REAL* q_buf, REAL* a_buf, REAL* b_buf)
{
    /*CIO::message(stderr, "prefetch called\n") ;*/
    if (bw)
    {
	forward_comp(p_observations->get_obs_T(dim), N-1, dim) ;
	backward_comp(p_observations->get_obs_T(dim), N-1, dim) ;
	REAL modprob=model_probability(dim) ;
	ab_buf_comp(p_buf, q_buf, a_buf, b_buf, dim) ;
	return modprob ;
    } 
#ifndef NOVIT
    else
	return best_path(dim) ;
#endif // NOVIT
} ;
#endif //PARALLEL


#ifdef PARALLEL

void CHMM::ab_buf_comp(REAL* p_buf, REAL* q_buf, REAL *a_buf, REAL* b_buf, int dim)
{
    int i,j,t ;
    REAL a_sum;
    REAL b_sum;
    REAL prob=0;	//model probability for dim
   
    REAL dimmodprob=model_probability(dim);

    for (i=0; i<N; i++)
    {
	//estimate initial+end state distribution numerator
	p_buf[i]=math.logarithmic_sum(get_p(i), get_p(i)+get_b(i,p_observations->get_obs(dim,0))+backward(0,i,dim) - dimmodprob);
	q_buf[i]=math.logarithmic_sum(get_q(i), forward(p_observations->get_obs_T(dim)-1, i, dim)+get_q(i) - dimmodprob);

	//estimate a
	for (j=0; j<N; j++)
	{
	    a_sum=-math.INFTY;

	    for (t=0; t<p_observations->get_obs_T(dim)-1; t++) 
	    {
		    a_sum= math.logarithmic_sum(a_sum, forward(t,i,dim)+
			    get_a(i,j)+get_b(j,p_observations->get_obs(dim,t+1))+backward(t+1,j,dim));
	    }
	    a_buf[N*i+j]=a_sum-dimmodprob ;
	}

	//estimate b
	for (j=0; j<M; j++)
	{
		b_sum=math.ALMOST_NEG_INFTY;

	    for (t=0; t<p_observations->get_obs_T(dim); t++) 
	    {
		    if (p_observations->get_obs(dim,t)==j) 
			b_sum=math.logarithmic_sum(b_sum, forward(t,i,dim)+backward(t, i, dim));
	    }

	    b_buf[M*i+j]=b_sum-dimmodprob ;
	}
    } 
}

//estimates new model lambda out of lambda_train using baum welch algorithm
void CHMM::estimate_model_baum_welch(CHMM* train)
{
    int i,j,t,cpu;
    REAL a_sum, b_sum;	//numerator
    REAL fullmodprob=0;	//for all dims

    //clear actual model a,b,p,q are used as numerator
    for (i=0; i<N; i++)
    {
	set_p(i,log(PSEUDO));
	set_q(i,log(PSEUDO));

	for (j=0; j<N; j++)
	    set_a(i,j, log(PSEUDO));
	for (j=0; j<M; j++)
	    set_b(i,j, log(PSEUDO));
    }

    pthread_t *threads=new pthread_t[NUM_PARALLEL] ;
    S_THREAD_PARAM *params=new S_THREAD_PARAM[NUM_PARALLEL] ;

    for (i=0; i<NUM_PARALLEL; i++)
    {
	params[i].p_buf=new REAL[N];
	params[i].q_buf=new REAL[N];
	params[i].a_buf=new REAL[N*N];
	params[i].b_buf=new REAL[N*M];
    } ;

    for (cpu=0; cpu<NUM_PARALLEL; cpu++)
    {
	/*CIO::message(stderr,"creating thread for dim=%i\n",dim+i) ;*/
	params[i].hmm=train;
	params[i].dim_start=p_observations->get_DIMENSION()*cpu / NUM_PARALLEL;
	params[i].dim_stop= p_observations->get_DIMENSION()*(cpu+1) / NUM_PARALLEL;

#ifdef SUNOS
	thr_create(NULL,0, bw_dim_prefetch, (void*)&params[i], PTHREAD_SCOPE_SYSTEM, &threads[i]) ;
#else // SUNOS
	pthread_create(&threads[i], NULL, bw_dim_prefetch, (void*)&params[i]) ;
#endif
    }

    for (cpu=0; cpu<NUM_PARALLEL; cpu++)
    {
	void* ret;
	pthread_join(threads[cpu], &ret) ;
	//dimmodprob = params[dim%NUM_PARALLEL].ret ;

	for (i=0; i<N; i++)
	{
	    //estimate initial+end state distribution numerator
	    set_p(i, math.logarithmic_sum(get_p(i), params[cpu].p_buf[i]));
	    set_q(i, math.logarithmic_sum(get_q(i), params[cpu].q_buf[i]));

	    //estimate numerator for a
	    for (j=0; j<N; j++)
		set_a(i,j, math.logarithmic_sum(get_a(i,j), params[cpu].a_buf[N*i+j]));

	    //estimate numerator for b
	    for (j=0; j<M; j++)
		set_b(i,j, math.logarithmic_sum(get_b(i,j), params[cpu].b_buf[M*i+j]));
	}

	fullmodprob+=params[cpu].prob;
    }
    
    for (i=0; i<NUM_PARALLEL; i++)
    {
	delete[] params[i].p_buf;
	delete[] params[i].q_buf;
	delete[] params[i].a_buf;
	delete[] params[i].b_buf;
    }

    delete[] threads ;
    delete[] params ;

    //cache train model probability
    train->mod_prob=fullmodprob;
    train->mod_prob_updated=true ;

    //new model probability is unknown
    normalize();
    invalidate_model();
}

#else // PARALLEL 

//estimates new model lambda out of lambda_train using baum welch algorithm
void CHMM::estimate_model_baum_welch(CHMM* train)
{
    int i,j,t,dim;
    REAL a_sum, b_sum;	//numerator
    REAL dimmodprob=0;	//model probability for dim
    REAL fullmodprob=0;	//for all dims

    //clear actual model a,b,p,q are used as numerator
    for (i=0; i<N; i++)
    {
	set_p(i,log(PSEUDO));
	set_q(i,log(PSEUDO));

	for (j=0; j<N; j++)
	    set_a(i,j, log(PSEUDO));
	for (j=0; j<M; j++)
	    set_b(i,j, log(PSEUDO));
    }

    //change summation order to make use of alpha/beta caches
    for (dim=0; dim<p_observations->get_DIMENSION(); dim++)
    {
	dimmodprob=train->model_probability(dim);
	fullmodprob+=dimmodprob ;

	for (i=0; i<N; i++)
	{
	    //estimate initial+end state distribution numerator
	    set_p(i, math.logarithmic_sum(get_p(i), train->get_p(i)+train->get_b(i,p_observations->get_obs(dim,0))+train->backward(0,i,dim) - dimmodprob));
	    set_q(i, math.logarithmic_sum(get_q(i), train->forward(p_observations->get_obs_T(dim)-1, i, dim)+train->get_q(i) - dimmodprob ));

	    //estimate a
	    for (j=0; j<N; j++)
	    {
		a_sum=-math.INFTY;

		for (t=0; t<p_observations->get_obs_T(dim)-1; t++) 
		{
		    a_sum= math.logarithmic_sum(a_sum, train->forward(t,i,dim)+
			    train->get_a(i,j)+train->get_b(j,p_observations->get_obs(dim,t+1))+train->backward(t+1,j,dim));
		}
		set_a(i,j, math.logarithmic_sum(get_a(i,j), a_sum-dimmodprob));
	    }

	    //estimate b
	    for (j=0; j<M; j++)
	    {
		b_sum=math.ALMOST_NEG_INFTY;

		for (t=0; t<p_observations->get_obs_T(dim); t++) 
		{
		    if (p_observations->get_obs(dim,t)==j) 
			b_sum=math.logarithmic_sum(b_sum, train->forward(t,i,dim)+train->backward(t, i, dim));
		}

		set_b(i,j, math.logarithmic_sum(get_b(i,j), b_sum-dimmodprob));
	    }
	} 
    }

    //cache train model probability
    train->mod_prob=fullmodprob;
    train->mod_prob_updated=true ;

    //new model probability is unknown
    normalize();
    invalidate_model();
}
#endif // PARALLEL


//estimates new model lambda out of lambda_train using baum welch algorithm
void CHMM::estimate_model_baum_welch_defined(CHMM* train)
{
    int i,j,old_i,k,t,dim;
    REAL a_sum_num, b_sum_num;		//numerator
    REAL a_sum_denom, b_sum_denom;	//denominator
    REAL dimmodprob=-math.INFTY;	//model probability for dim
    REAL fullmodprob=0;			//for all dims
    REAL* A=ARRAYN1(0);
    REAL* B=ARRAYN2(0);
    
    //clear actual model a,b,p,q are used as numerator
    //A,B as denominator for a,b
    for (k=0; (i=model->get_learn_p(k))!=-1; k++)	
	set_p(i,log(PSEUDO));

    for (k=0; (i=model->get_learn_q(k))!=-1; k++)	
	set_q(i,log(PSEUDO));

    for (k=0; (i=model->get_learn_a(k,0))!=-1; k++)
    {
	j=model->get_learn_a(k,1);
	set_a(i,j, log(PSEUDO));
    }

    for (k=0; (i=model->get_learn_b(k,0))!=-1; k++)
    {
	j=model->get_learn_b(k,1);
	set_b(i,j, log(PSEUDO));
    }

    for (i=0; i<N; i++)
    {
	A[i]=log(PSEUDO);
	B[i]=log(PSEUDO);
    }

#ifdef PARALLEL
    pthread_t *threads=new pthread_t[NUM_PARALLEL] ;
    S_THREAD_PARAM *params=new S_THREAD_PARAM[NUM_PARALLEL] ;
#endif

    //change summation order to make use of alpha/beta caches
    for (dim=0; dim<p_observations->get_DIMENSION(); dim++)
    {
#ifdef PARALLEL
      if (dim%NUM_PARALLEL==0)
	{
	  int i ;
	  for (i=0; i<NUM_PARALLEL; i++)
	    if (dim+i<p_observations->get_DIMENSION())
	      {
		/*CIO::message(stderr,"creating thread for dim=%i\n",dim+i) ;*/
		params[i].hmm=train ;
		params[i].dim=dim+i ;
#ifdef SUNOS
		thr_create(NULL,0, bw_dim_prefetch, (void*)&params[i], PTHREAD_SCOPE_SYSTEM, &threads[i]) ;
#else // SUNOS
		pthread_create(&threads[i], NULL, bw_dim_prefetch, (void*)&params[i]) ;
#endif
	      } ;
	  for (i=0; i<NUM_PARALLEL; i++)
	    if (dim+i<p_observations->get_DIMENSION())
	      {
		void * ret ;
		pthread_join(threads[i], &ret) ;
		dimmodprob = params[i].ret ;
		/*CIO::message(stderr,"thread for dim=%i returned: %e\n",dim+i,dimmodprob) ;*/
	      } ;
	}
#else
      dimmodprob=train->model_probability(dim);
#endif // PARALLEL

      //and denominator
      fullmodprob+= dimmodprob;
	    
	//estimate initial+end state distribution numerator
	for (k=0; (i=model->get_learn_p(k))!=-1; k++)	
	    set_p(i, math.logarithmic_sum(get_p(i), train->forward(0,i,dim)+train->backward(0,i,dim) - dimmodprob ) );

	for (k=0; (i=model->get_learn_q(k))!=-1; k++)	
	    set_q(i, math.logarithmic_sum(get_q(i), train->forward(p_observations->get_obs_T(dim)-1, i, dim)+
			train->backward(p_observations->get_obs_T(dim)-1, i, dim)  - dimmodprob ) );

	//estimate a
	old_i=-1;
	for (k=0; (i=model->get_learn_a(k,0))!=-1; k++)
	{
	    //denominator is constant for j
	    //therefore calculate it first
	    if (old_i!=i)
	    {
		old_i=i;
		a_sum_denom=-math.INFTY;

		for (t=0; t<p_observations->get_obs_T(dim)-1; t++) 
		    a_sum_denom= math.logarithmic_sum(a_sum_denom, train->forward(t,i,dim)+train->backward(t,i,dim));

		A[i]= math.logarithmic_sum(A[i], a_sum_denom-dimmodprob);
	    }
	    
	    j=model->get_learn_a(k,1);
	    a_sum_num=-math.INFTY;
	    for (t=0; t<p_observations->get_obs_T(dim)-1; t++) 
	    {
		a_sum_num= math.logarithmic_sum(a_sum_num, train->forward(t,i,dim)+
			train->get_a(i,j)+train->get_b(j,p_observations->get_obs(dim,t+1))+train->backward(t+1,j,dim));
	    }

	    set_a(i,j, math.logarithmic_sum(get_a(i,j), a_sum_num-dimmodprob));
	}
	
	//estimate  b
	old_i=-1;
	for (k=0; (i=model->get_learn_b(k,0))!=-1; k++)
	{

	    //denominator is constant for j
	    //therefore calculate it first
	    if (old_i!=i)
	    {
		old_i=i;
		b_sum_denom=-math.INFTY;

		for (t=0; t<p_observations->get_obs_T(dim); t++) 
		    b_sum_denom= math.logarithmic_sum(b_sum_denom, train->forward(t,i,dim)+train->backward(t,i,dim));

		B[i]= math.logarithmic_sum(B[i], b_sum_denom-dimmodprob);
	    }

	    j=model->get_learn_b(k,1);
	    b_sum_num=-math.INFTY;
	    for (t=0; t<p_observations->get_obs_T(dim); t++) 
	    {
		if (p_observations->get_obs(dim,t)==j) 
		    b_sum_num=math.logarithmic_sum(b_sum_num, train->forward(t,i,dim)+train->backward(t, i, dim));
	    }

	    set_b(i,j, math.logarithmic_sum(get_b(i,j), b_sum_num-dimmodprob));
	}
    }
#ifdef PARALLEL
    delete[] threads ;
    delete[] params ;
#endif


    //calculate estimates
    for (k=0; (i=model->get_learn_p(k))!=-1; k++)	
		set_p(i, get_p(i)-log(p_observations->get_DIMENSION()+N*PSEUDO) );

    for (k=0; (i=model->get_learn_q(k))!=-1; k++)	
		set_q(i, get_q(i)-log(p_observations->get_DIMENSION()+N*PSEUDO) );

    for (k=0; (i=model->get_learn_a(k,0))!=-1; k++)
    {
		j=model->get_learn_a(k,1);
		set_a(i,j, get_a(i,j) - A[i]);
    }

    for (k=0; (i=model->get_learn_b(k,0))!=-1; k++)
    {
		j=model->get_learn_b(k,1);
		set_b(i,j, get_b(i,j) - B[i]);
    }

    //cache train model probability
    train->mod_prob=fullmodprob;
    train->mod_prob_updated=true ;
    
    //new model probability is unknown
    normalize();
    invalidate_model();
}

#ifndef NOVIT
//estimates new model lambda out of lambda_train using viterbi algorithm
void CHMM::estimate_model_viterbi(CHMM* train)
{
	int i,j,t;
	REAL sum;
	REAL* P=ARRAYN1(0);
	REAL* Q=ARRAYN2(0);

	path_deriv_updated=false ;

	//initialize with pseudocounts
	for (i=0; i<N; i++)
	{
		for (j=0; j<N; j++)
			set_A(i,j, PSEUDO);

		for (j=0; j<M; j++)
			set_B(i,j, PSEUDO);

		P[i]=PSEUDO;
		Q[i]=PSEUDO;
	}

	REAL allpatprob=0 ;

#ifdef PARALLEL
	pthread_t *threads=new pthread_t[NUM_PARALLEL] ;
	S_THREAD_PARAM *params=new S_THREAD_PARAM[NUM_PARALLEL] ;
#endif

	for (int dim=0; dim<p_observations->get_DIMENSION(); dim++)
	{

#ifdef PARALLEL
	  if (dim%NUM_PARALLEL==0)
	    {
	      int i ;
	      for (i=0; i<NUM_PARALLEL; i++)
		if (dim+i<p_observations->get_DIMENSION())
		  {
		    /*CIO::message(stderr,"creating thread for dim=%i\n",dim+i) ;*/
		    params[i].hmm=train ;
		    params[i].dim=dim+i ;
#ifdef SUNOS
		    thr_create(NULL,0, vit_dim_prefetch, (void*)&params[i], PTHREAD_SCOPE_SYSTEM, &threads[i]) ;
#else // SUNOS
		    pthread_create(&threads[i], NULL, vit_dim_prefetch, (void*)&params[i]) ;
#endif
		  } ;
	      for (i=0; i<NUM_PARALLEL; i++)
		if (dim+i<p_observations->get_DIMENSION())
		  {
		    void * ret ;
		    pthread_join(threads[i], &ret) ;
		    allpatprob += params[i].ret ;
		    /*CIO::message(stderr,"thread for dim=%i returned: %e\n",dim+i,params[i].ret) ;*/
		  } ;
	    } ;
#else
	  //using viterbi to find best path
	  allpatprob += train->best_path(dim);
#endif // PARALLEL
	  
	  //counting occurences for A and B
	  for (t=0; t<p_observations->get_obs_T(dim)-1; t++)
	    {
	      set_A(train->PATH(dim)[t], train->PATH(dim)[t+1], get_A(train->PATH(dim)[t], train->PATH(dim)[t+1])+1);
	      set_B(train->PATH(dim)[t], p_observations->get_obs(dim,t),  get_B(train->PATH(dim)[t], p_observations->get_obs(dim,t))+1);
	    }
	  
	  set_B(train->PATH(dim)[p_observations->get_obs_T(dim)-1], p_observations->get_obs(dim,p_observations->get_obs_T(dim)-1),  get_B(train->PATH(dim)[p_observations->get_obs_T(dim)-1], p_observations->get_obs(dim,p_observations->get_obs_T(dim)-1)) + 1 );
	  
	  P[train->PATH(dim)[0]]++;
	  Q[train->PATH(dim)[p_observations->get_obs_T(dim)-1]]++;
	}

#ifdef PARALLEL
	delete[] threads ;
	delete[] params ;
#endif 

	allpatprob/=p_observations->get_DIMENSION() ;
	train->all_pat_prob=allpatprob ;
	train->all_path_prob_updated=true ;

	//converting A to probability measure a
	for (i=0; i<N; i++)
	  {
	    sum=0;
	    for (j=0; j<N; j++)
	      sum+=get_A(i,j);
	    
	    for (j=0; j<N; j++)
	      set_a(i,j, log(get_A(i,j)/sum));
	  }
	
	//converting B to probability measures b
	for (i=0; i<N; i++)
	  {
	    sum=0;
	    for (j=0; j<M; j++)
	      sum+=get_B(i,j);
	    
	    for (j=0; j<M; j++)
	      set_b(i,j, log(get_B(i, j)/sum));
	  }
	
	//converting P to probability measure p
	sum=0;
	for (i=0; i<N; i++)
	  sum+=P[i];
	
	for (i=0; i<N; i++)
	  set_p(i, log(P[i]/sum));
	
	//converting Q to probability measure q
	sum=0;
	for (i=0; i<N; i++)
	  sum+=Q[i];
	
	for (i=0; i<N; i++)
	  set_q(i, log(Q[i]/sum));
	
	//new model probability is unknown
	invalidate_model();
}

// estimate parameters listed in learn_x
void CHMM::estimate_model_viterbi_defined(CHMM* train)
{
	int i,j,k,t;
	REAL sum;
	REAL* P=ARRAYN1(0);
	REAL* Q=ARRAYN2(0);

	path_deriv_updated=false ;

	//initialize with pseudocounts
	for (i=0; i<N; i++)
	{
	  for (j=0; j<N; j++)
	    set_A(i,j, PSEUDO);
	  
	  for (j=0; j<M; j++)
	    set_B(i,j, PSEUDO);
	  
	  P[i]=PSEUDO;
	  Q[i]=PSEUDO;
	}

#ifdef PARALLEL
	pthread_t *threads=new pthread_t[NUM_PARALLEL] ;
	S_THREAD_PARAM *params=new S_THREAD_PARAM[NUM_PARALLEL] ;
#endif

	REAL allpatprob=0.0 ;
	for (int dim=0; dim<p_observations->get_DIMENSION(); dim++)
	{
	  
#ifdef PARALLEL
	  if (dim%NUM_PARALLEL==0)
	    {
	      int i ;
	      for (i=0; i<NUM_PARALLEL; i++)
		if (dim+i<p_observations->get_DIMENSION())
		  {
		    /*		    CIO::message(stderr,"creating thread for dim=%i\n",dim+i) ;*/
		    params[i].hmm=train ;
		    params[i].dim=dim+i ;
#ifdef SUNOS
		    thr_create(NULL,0,vit_dim_prefetch, (void*)&params[i], PTHREAD_SCOPE_SYSTEM, &threads[i]) ;
#else // SUNOS
		    pthread_create(&threads[i], NULL, vit_dim_prefetch, (void*)&params[i]) ;
#endif //SUNOS
		  } ;
	      for (i=0; i<NUM_PARALLEL; i++)
		if (dim+i<p_observations->get_DIMENSION())
		  {
		    void * ret ;
		    pthread_join(threads[i], &ret) ;
		    allpatprob += params[i].ret ;
		    /*		    CIO::message(stderr,"thread for dim=%i returned: %e\n",dim+i,params[i].ret) ;*/
		  } ;
	    } ;
#else // PARALLEL
	  //using viterbi to find best path
	  allpatprob += train->best_path(dim);
#endif // PARALLEL

	  
	  //counting occurences for A and B
	  for (t=0; t<p_observations->get_obs_T(dim)-1; t++)
	    {
	      set_A(train->PATH(dim)[t], train->PATH(dim)[t+1], get_A(train->PATH(dim)[t], train->PATH(dim)[t+1])+1);
	      set_B(train->PATH(dim)[t], p_observations->get_obs(dim,t),  get_B(train->PATH(dim)[t], p_observations->get_obs(dim,t))+1);
	    }
	  
	  set_B(train->PATH(dim)[p_observations->get_obs_T(dim)-1], p_observations->get_obs(dim,p_observations->get_obs_T(dim)-1),  get_B(train->PATH(dim)[p_observations->get_obs_T(dim)-1], p_observations->get_obs(dim,p_observations->get_obs_T(dim)-1)) + 1 );
	  
	  P[train->PATH(dim)[0]]++;
	  Q[train->PATH(dim)[p_observations->get_obs_T(dim)-1]]++;
	}

#ifdef PARALLEL
	delete[] threads ;
	delete[] params ;
#endif

	//train->invalidate_model() ;
	//REAL q=train->best_path(-1) ;

	allpatprob/=p_observations->get_DIMENSION() ;
	train->all_pat_prob=allpatprob ;
	train->all_path_prob_updated=true ;

	//CIO::message(stderr, "q=%e, tt=%e\n", q, allpatprob) ;

	//copy old model
	for (i=0; i<N; i++)
	{
		for (j=0; j<N; j++)
			set_a(i,j, train->get_a(i,j));

		for (j=0; j<M; j++)
			set_b(i,j, train->get_b(i,j));
	}

	//converting A to probability measure a
	i=0;
	sum=0;
	j=model->get_learn_a(i,0);
	k=i;
	while (model->get_learn_a(i,0)!=-1 || k<i)
	{
		if (j==model->get_learn_a(i,0))
		{
			sum+=get_A(model->get_learn_a(i,0), model->get_learn_a(i,1));
			i++;
		}
		else
		{
			while (k<i)
			{
				set_a(model->get_learn_a(k,0), model->get_learn_a(k,1), log (get_A(model->get_learn_a(k,0), model->get_learn_a(k,1)) / sum));
				k++;
			}

			sum=0;
			j=model->get_learn_a(i,0);
			k=i;
		}
	}
		
	//converting B to probability measures b
	i=0;
	sum=0;
	j=model->get_learn_b(i,0);
	k=i;
	while (model->get_learn_b(i,0)!=-1 || k<i)
	{
		if (j==model->get_learn_b(i,0))
		{
			sum+=get_B(model->get_learn_b(i,0),model->get_learn_b(i,1));
			i++;
		}
		else
		{
			while (k<i)
			{
				set_b(model->get_learn_b(k,0),model->get_learn_b(k,1), log (get_B(model->get_learn_b(k,0), model->get_learn_b(k,1)) / sum));
				k++;
			}
			
			sum=0;
			j=model->get_learn_b(i,0);
			k=i;
		}
	}

	i=0;
	sum=0;
	while (model->get_learn_p(i)!=-1)
	{
	  sum+=P[model->get_learn_p(i)] ;
	  i++ ;
	} ;
	i=0 ;
	while (model->get_learn_p(i)!=-1)
	{
	  set_p(model->get_learn_p(i), log(P[model->get_learn_p(i)]/sum));
	  i++ ;
	} ;

	i=0;
	sum=0;
	while (model->get_learn_q(i)!=-1)
	{
	  sum+=Q[model->get_learn_q(i)] ;
	  i++ ;
	} ;
	i=0 ;
	while (model->get_learn_q(i)!=-1)
	{
	  set_q(model->get_learn_q(i), log(Q[model->get_learn_q(i)]/sum));
	  i++ ;
	} ;

	
	//new model probability is unknown
	invalidate_model();
}
#endif // NOVIT

//------------------------------------------------------------------------------------//

//to give an idea what the model looks like
void CHMM::output_model(bool verbose)
{
	int i,j;
	REAL checksum;
	
	//generic info
	printf("log(Pr[O|model])=%e, #states: %i, #observationssymbols: %i, #observations: %ix%i\n", 
	       (double)((p_observations) ? model_probability() : -math.INFTY), 
	       N, M, p_observations->get_obs_max_T(),p_observations->get_DIMENSION());

	if (verbose)
	{
		// tranisition matrix a
		printf("\ntransition matrix\n");
		for (i=0; i<N; i++)
		{
			checksum=-math.INFTY;
			for (j=0; j<N; j++)
			{
				checksum= math.logarithmic_sum(checksum, get_a(i,j));

				printf("a(%02i,%02i)=%1.4f ",i,j, (float) exp(get_a(i,j)));
				
				if (j % 4 == 3)
					printf("\n");
			}
			if (fabs(checksum)>1e-5)
			 CIO::message(" checksum % E ******* \n",checksum);
			else
			 CIO::message(" checksum % E\n",checksum);
		}

		// distribution of start states p
		printf("\ndistribution of start states\n");
		checksum=-math.INFTY;
		for (i=0; i<N; i++)
		{
			checksum= math.logarithmic_sum(checksum, get_p(i));
			printf("p(%02i)=%1.4f ",i, (float) exp(get_p(i)));
			if (i % 4 == 3)
				printf("\n");
		}
		if (fabs(checksum)>1e-5)
		 CIO::message(" checksum % E ******* \n",checksum);
		else
		 CIO::message(" checksum=% E\n", checksum);

		// distribution of terminal states p
		printf("\ndistribution of terminal states\n");
		checksum=-math.INFTY;
		for (i=0; i<N; i++)
		{
			checksum= math.logarithmic_sum(checksum, get_q(i));
			printf("q(%02i)=%1.4f ",i, (float) exp(get_q(i)));
			if (i % 4 == 3)
				printf("\n");
		}
		if (fabs(checksum)>1e-5)
		 CIO::message(" checksum % E ******* \n",checksum);
		else
		 CIO::message(" checksum=% E\n", checksum);

		// distribution of observations given the state b
		printf("\ndistribution of observations given the state\n");
		for (i=0; i<N; i++)
		{
			checksum=-math.INFTY;
			for (j=0; j<M; j++)
			{
				checksum=math.logarithmic_sum(checksum, get_b(i,j));
				printf("b(%02i,%02i)=%1.4f ",i,j, (float) exp(get_b(i,j)));
				if (j % 4 == 3)
					printf("\n");
			}
			if (fabs(checksum)>1e-5)
			 CIO::message(" checksum % E ******* \n",checksum);
			else
			 CIO::message(" checksum % E\n",checksum);
		}
	}
	printf("\n");
}

//to give an idea what the model looks like
void CHMM::output_model_defined(bool verbose)
{
	int i,j;
	if (!model)
	  return ;

	//generic info
	printf("log(Pr[O|model])=% E, #states: %i, #observationssymbols: %i, #observations: %i\n",
	       ((double) ((p_observations) ? model_probability(): -math.INFTY)), N, M, p_observations->get_obs_max_T());

	if (verbose)
	{
		// tranisition matrix a
		printf("\ntransition matrix\n");

		//initialize a values that have to be learned
		i=0;
		j=model->get_learn_a(i,0);
		while (model->get_learn_a(i,0)!=-1)
		{
			if (j!=model->get_learn_a(i,0))
			{
				j=model->get_learn_a(i,0);
				printf("\n");
			}

			printf("a(%02i,%02i)=%1.4f ",model->get_learn_a(i,0), model->get_learn_a(i,1), (float) exp(get_a(model->get_learn_a(i,0), model->get_learn_a(i,1))));
			i++;
		}
			
		// distribution of observations given the state b
		printf("\n\ndistribution of observations given the state\n");
		i=0;
		j=model->get_learn_b(i,0);
		while (model->get_learn_b(i,0)!=-1)
		{
			if (j!=model->get_learn_b(i,0))
			{
				j=model->get_learn_b(i,0);
				printf("\n");
			}

			printf("b(%02i,%02i)=%1.4f ",model->get_learn_b(i,0),model->get_learn_b(i,1), (float) exp(get_b(model->get_learn_b(i,0),model->get_learn_b(i,1))));
			i++;
		}
		
		printf("\n");
	}
	printf("\n");
}

#ifndef NOVIT
//output the path
void CHMM::output_model_sequence(bool verbose, int from, int to)
{
	int j,k=0,diffs=0;
	REAL maxj;
	unsigned char output;
	REAL expected_sequence_prob=0 ;

	
	from=math.min(math.max(0,from),p_observations->get_DIMENSION()) ;
	to=math.min(math.max(0,to),p_observations->get_DIMENSION()) ;
	
	//get path
	printf("path: ");
	int q;
	for (q=0; q<p_observations->get_DIMENSION(); q++)
	  {
	    expected_sequence_prob+=best_path(q);
	    for (j=0; j<p_observations->get_obs_T(q); j++)
	      {
		maxj=get_b(PATH(q)[j], 0);
		output=0;
		
		for (k=0; k<M; k++)
		  {
		    if (get_b(PATH(q)[j], k)>maxj)
		      {
			maxj=get_b(PATH(q)[j],k);
			output=k;
		      }
		  }
		if (p_observations->get_obs(q,j)!=output)
		  diffs++;
	      }
	  }
	expected_sequence_prob/=p_observations->get_DIMENSION() ;
	
	printf("\nE log(Pr[path|model])=% E #wrong symbols: %d (%d) that is %f Percent\n\n", (double) expected_sequence_prob, diffs, p_observations->get_obs_max_T()*p_observations->get_DIMENSION(), 100.0*diffs/(p_observations->get_obs_max_T()*p_observations->get_DIMENSION()));
	
	if (verbose)
	  {
	    int q,i ;
	    const int outlen_max=math.min(p_observations->get_obs_max_T()+1,200) ;
	    char *buf=new char[(ORDER+2)*outlen_max];
	    for (q=from; q<to; q++)
	      {
		const int outlen=math.min(p_observations->get_obs_T(q)+1,200) ;
		
		for (i=0; i<=(ORDER+1); i++) 
		  buf[outlen*(i+1)-1]='\x0';
		
		REAL pr=best_path(q);		
		printf("sequence %i: norm_log_prob=%e\n", q, (double) pr) ;
		
		for (j=0; j<outlen-1; j++)
		  {
		    maxj=get_b(PATH(q)[j], 0);
		    output=0;
		    
		    for (k=0; k<M; k++)
		      {
			if (get_b(PATH(q)[j],k)>maxj)
			  {
			    maxj=get_b(PATH(q)[j],k);
			    output=k;
			  }
		      }
		    
		    for (i=0; i<ORDER; i++)
		      buf[(i+1)*outlen+j]=p_observations->remap((output & (((1 << MAX_M) -1 )<<(MAX_M*i)))>>(MAX_M*i)) ;
		    if (PATH(q)[j]<10)
		      buf[j]='0'+PATH(q)[j] ;
		    else
		      buf[j]='a'-10+PATH(q)[j] ;
		  }
		
		if (p_observations->get_obs_T(q)>outlen)
		 CIO::message("%s...\n",buf) ; 
		else
		 CIO::message("%s\n",buf) ; 
		
		for (j=0; j<outlen-1; j++) 
		 CIO::message("-"); 
		printf("\n");
		
		for (i=1; i<ORDER+1; i++) 
		  if (p_observations->get_obs_T(q)>outlen)
		   CIO::message("%s...\n",&buf[outlen*i]); 
		  else
		   CIO::message("%s\n",&buf[outlen*i]); 
		
		for (j=0; j<outlen-1; j++)
		 CIO::message("-"); 
		printf("\n") ;
		
		for (j=0; j<outlen-1; j++)
		  {
		    maxj=get_b(PATH(q)[j], 0);
		    output=0;
		    
		    for (k=0; k<M; k++)
		      if (get_b(PATH(q)[j], k)>maxj)
			{
			  maxj=get_b(PATH(q)[j],k);
			  output=k;
			}
		    if ((p_observations->get_obs(q,j) & ((1 << MAX_M ) -1 )) != (output & ((1 << MAX_M ) -1 ) ))
		     CIO::message("%c",p_observations->remap(p_observations->get_obs(q,j) & ((1 << MAX_M ) -1 ) ));
		    else
		     CIO::message(" ");
		  }
		if (p_observations->get_obs_T(q)>outlen)
		 CIO::message("...\n") ; 
		else
		 CIO::message("\n") ; 
		
		for (j=0; j<outlen-1; j++) 
		 CIO::message("-"); 
		printf("\n") ;
		
		for (j=0; j<outlen-1; j++)
		 CIO::message("%c",p_observations->remap(p_observations->get_obs(q,j) & ((1 << MAX_M ) -1 ) ));
		if (p_observations->get_obs_T(q)>outlen)
		 CIO::message("...\n\n") ; 
		else
		 CIO::message("\n\n") ; 
		
		if (p_observations->get_obs_T(q)>outlen)
		  {
		    for (j=0; j<outlen-1; j++)
		      {
			maxj=get_b(PATH(q)[j+p_observations->get_obs_T(q)-outlen], 0);
			output=0;
			for (k=0; k<M; k++)
			  {
			    if (get_b(PATH(q)[j+p_observations->get_obs_T(q)-outlen],k)>maxj)
			      {
				maxj=get_b(PATH(q)[j+p_observations->get_obs_T(q)-outlen],k);
				output=k;
			      }
			  }
			
			for (i=0; i<ORDER; i++)
			  buf[(i+1)*outlen+j]=p_observations->remap((output & (((1 << MAX_M) -1 )<<(MAX_M*i)))>>(MAX_M*i)) ;
			
			if (PATH(q)[j]<10)
			  buf[j]='0'+PATH(q)[j] ;
			else
			  buf[j]='a'-10+PATH(q)[j] ;
		      }
		   CIO::message("...%s\n   ",buf) ; 
		    for (j=0; j<outlen-1; j++)
		     CIO::message("-");
		   CIO::message("\n") ;
		    
		    for (i=1; i<ORDER+1; i++) 
		     CIO::message("...%s\n",&buf[outlen*i]) ; 
		   CIO::message("   ") ;
		    
		    for (j=0; j<outlen-1; j++)
		     CIO::message("-");
		   CIO::message("\n...") ;
		    
		    for (j=0; j<outlen-1; j++)
		      {
			maxj=get_b(PATH(q)[j+p_observations->get_obs_T(q)-outlen], 0);
			output=k;
			
			for (k=0; k<M; k++)
			  {
			    if (get_b(PATH(q)[j+p_observations->get_obs_T(q)-outlen], k)>maxj)
			      {
				maxj=get_b(PATH(q)[j+p_observations->get_obs_T(q)-outlen],k);
				output=k;
			      }
			  }
			
			if ((p_observations->get_obs(q,j+p_observations->get_obs_T(q)-outlen) & ((1 << MAX_M ) -1 )) != (output & ((1 << MAX_M ) -1 ) ))
			 CIO::message("%c",p_observations->remap(p_observations->get_obs(q,j+p_observations->get_obs_T(q)-outlen) & ((1 << MAX_M ) -1 ) ));
			else
			 CIO::message(" ");
		      }
		   CIO::message("\n   ") ; 
		    
		    for (j=0; j<outlen-1; j++) 
		     CIO::message("-"); 
		   CIO::message("\n...") ;
		    
		    for (j=0; j<outlen-1; j++)
		     CIO::message("%c",p_observations->remap(p_observations->get_obs(q,j+p_observations->get_obs_T(q)-outlen) & ((1 << MAX_M ) -1 ) ));
		   CIO::message("\n\n") ;
		  }
	      } ;
	    delete[] buf;
	  }
}

//output gene-positions
void CHMM::output_gene_positions(bool verbose)
{
	int t,o;

	//get path
	best_path(0);

	//output state sequence
	printf("\nmodel estimated the DNA to look like the following:\n\n ");


	printf("<START");
	o=-1;
	for (t=0; t<p_observations->get_obs_T(0)-1; t++)
	{
		if (o!=PATH(0)[t])
		{
			if (PATH(0)[t]==0)		//intergenic state
			{
				printf(">%d<--INTERGENIC--",t);
			} 
			else if (PATH(0)[t]==9)	//genic state
			{
				printf(">%d<--GENE--",t);
			}
			o=PATH(0)[t];
		}
	}
	printf("><END>");

}
#endif // NOVIT

//------------------------------------------------------------------------------------//

//convert model to log probabilities
void CHMM::convert_to_log()
{
	int i,j;

	for (i=0; i<N; i++)
	{
		if (get_p(i)!=0)
			set_p(i, log(get_p(i)));
		else
			set_p(i, -math.INFTY);;
	}

	for (i=0; i<N; i++)
	{
		if (get_q(i)!=0)
			set_q(i, log(get_q(i)));
		else
			set_q(i, -math.INFTY);;
	}

	for (i=0; i<N; i++)
	{
		for (j=0; j<N; j++)
		{
			if (get_a(i,j)!=0)
				set_a(i,j, log(get_a(i,j)));
			else
				set_a(i,j, -math.INFTY);
		}
	}

	for (i=0; i<N; i++)
	{
		for (j=0; j<M; j++)
		{
			if (get_b(i,j)!=0)
				set_b(i,j, log(get_b(i,j)));
			else
				set_b(i,j, -math.INFTY);
		}
	}
	loglikelihood=true;

	invalidate_model();
}

//init model with random values
void CHMM::init_model_random()
{
	const REAL MIN_RAND=1e-1;
	srand( (unsigned)time( NULL ) );

	REAL sum;
	int i,j;

	//initialize a with random values
	for (i=0; i<N; i++)
	{
		sum=0;
		for (j=0; j<N; j++)
		{
			set_a(i,j, (MIN_RAND+((REAL)rand()))/REAL(RAND_MAX));

			sum+=get_a(i,j);
		}

		for (j=0; j<N; j++)
			set_a(i,j, get_a(i,j)/sum);
	}

	//initialize pi with random values
	sum=0;
	for (i=0; i<N; i++)
	{
			set_p(i, (MIN_RAND+((REAL)rand()))/REAL(RAND_MAX));

			sum+=get_p(i);
	}

	for (i=0; i<N; i++)
		set_p(i, get_p(i)/sum);

	//initialize q with random values
	sum=0;
	for (i=0; i<N; i++)
	{
			set_q(i, (MIN_RAND+((REAL)rand()))/REAL(RAND_MAX));

			sum+=get_q(i);
	}

	for (i=0; i<N; i++)
		set_q(i, get_q(i)/sum);

	//initialize b with random values
	for (i=0; i<N; i++)
	{
		sum=0;
		for (j=0; j<M; j++)
		{
			set_b(i,j, (MIN_RAND+((REAL)rand()))/REAL(RAND_MAX));

			sum+=get_b(i,j);
		}

		for (j=0; j<M; j++)
			set_b(i,j, get_b(i,j)/sum);
	}

	//initialize pat/mod_prob as not calculated
	invalidate_model();
}

//init model according to const_x
void CHMM::init_model_defined()
{
	int i,j,k,r;
	REAL sum;

	//initialize a with zeros
	for (i=0; i<N; i++)
		for (j=0; j<N; j++)
			set_a(i,j, 0);

	//initialize p with zeros
	for (i=0; i<N; i++)
	  set_p(i, 0);

	//initialize q with zeros
	for (i=0; i<N; i++)
			set_q(i, 0);

	//initialize b with zeros
	for (i=0; i<N; i++)
		for (j=0; j<M; j++)
			set_b(i,j, 0);


	//initialize a values that have to be learned
	REAL *R=new REAL[N] ;
	for (r=0; r<N; r++) R[r]=(1e-4+((REAL)rand()))/REAL(RAND_MAX) ;
	i=0; sum=0; k=i; 
	j=model->get_learn_a(i,0);
	while (model->get_learn_a(i,0)!=-1 || k<i)
	{
	  if (j==model->get_learn_a(i,0))
	    {
	      sum+=R[model->get_learn_a(i,1)] ;
	      i++;
	    }
	  else
	    {
	      while (k<i)
		{
		  set_a(model->get_learn_a(k,0), model->get_learn_a(k,1), 
			R[model->get_learn_a(k,1)]/sum);
		  k++;
		}
	      j=model->get_learn_a(i,0);
	      k=i;
	      sum=0;
	      for (int r=0; r<N; r++) R[r]=(1e-4+((REAL)rand()))/REAL(RAND_MAX) ;
	    }
	}
	delete[] R ; R=NULL ;
		
	//initialize b values that have to be learned
	R=new REAL[M] ;
	for (r=0; r<M; r++) R[r]=(1e-4+((REAL)rand()))/REAL(RAND_MAX) ;
	i=0; sum=0; k=0 ;
	j=model->get_learn_b(i,0);
	while (model->get_learn_b(i,0)!=-1 || k<i)
	  {
	    if (j==model->get_learn_b(i,0))
	      {
		sum+=R[model->get_learn_b(i,1)] ;
		i++;
	      }
	    else
	      {
		while (k<i)
		  {
		    set_b(model->get_learn_b(k,0),model->get_learn_b(k,1), 
			  R[model->get_learn_b(k,1)]/sum);
		    k++;
		  }
		
		j=model->get_learn_b(i,0);
		k=i;
		sum=0;
		for (int r=0; r<M; r++) R[r]=(1e-4+((REAL)rand()))/REAL(RAND_MAX) ;
	      }
	  }
	delete[] R ; R=NULL ;
	
	//set consts into a
	i=0;
	while (model->get_const_a(i,0) != -1)
	{
		set_a(model->get_const_a(i,0), model->get_const_a(i,1), model->get_const_a_val(i));
		i++;
	}

	//set consts into b
	i=0;
	while (model->get_const_b(i,0) != -1)
	{
		set_b(model->get_const_b(i,0), model->get_const_b(i,1), model->get_const_b_val(i));
		i++;
	}

	//set consts into p
	i=0;
	while (model->get_const_p(i) != -1)
	{
		set_p(model->get_const_p(i), model->get_const_p_val(i));
		i++;
	}

	//initialize q with zeros
	for (i=0; i<N; i++)
	  set_q(i, 0.0);

	//set consts into q
	i=0;
	while (model->get_const_q(i) != -1)
	{
		set_q(model->get_const_q(i), model->get_const_q_val(i));
		i++;
	}

	// init p
	i=0;
	sum=0;
	while (model->get_learn_p(i)!=-1)
	{
	  set_p(model->get_learn_p(i),(1e-4+((REAL)rand()))/((REAL)RAND_MAX)) ;
	  sum+=get_p(model->get_learn_p(i)) ;
	  i++ ;
	} ;
	i=0 ;
	while (model->get_learn_p(i)!=-1)
	{
	  set_p(model->get_learn_p(i), get_p(model->get_learn_p(i))/sum);
	  i++ ;
	} ;

	// initialize q
	i=0;
	sum=0;
	while (model->get_learn_q(i)!=-1)
	{
	  set_q(model->get_learn_q(i),(1e-4+((REAL)rand()))/((REAL)RAND_MAX)) ;
	  sum+=get_q(model->get_learn_q(i)) ;
	  i++ ;
	} ;
	i=0 ;
	while (model->get_learn_q(i)!=-1)
	{
	  set_q(model->get_learn_q(i), get_q(model->get_learn_q(i))/sum);
	  i++ ;
	} ;

	//initialize pat/mod_prob as not calculated
	invalidate_model();
}

void CHMM::clear_model()
{
  int i,j;
  for (i=0; i<N; i++)
    {
      set_p(i, log(PSEUDO));
      set_q(i, log(PSEUDO));
      
      for (j=0; j<N; j++)
	set_a(i,j, log(PSEUDO));
      
      for (j=0; j<M; j++)
	set_b(i,j, log(PSEUDO));
    }
}

void CHMM::clear_model_defined()
{
  int i,j,k;
  
  for (i=0; (j=model->get_learn_p(i))!=-1; i++)
    set_p(j, log(PSEUDO));
  
  for (i=0; (j=model->get_learn_q(i))!=-1; i++)
    set_q(j, log(PSEUDO));
  
  for (i=0; (j=model->get_learn_a(i,0))!=-1; i++)
    {
      k=model->get_learn_a(i,1); // catch (j,k) as indizes to be learned
      set_a(j,k, log(PSEUDO));
    }
  
  for (i=0; (j=model->get_learn_b(i,0))!=-1; i++)
    {
      k=model->get_learn_b(i,1); // catch (j,k) as indizes to be learned
      set_b(j,k, log(PSEUDO));
    }
}

void CHMM::copy_model(CHMM* l)
{
  int i,j;
  for (i=0; i<N; i++)
    {
      set_p(i, l->get_p(i));
      set_q(i, l->get_q(i));
      
      for (j=0; j<N; j++)
	set_a(i,j, l->get_a(i,j));
      
      for (j=0; j<M; j++)
	set_b(i,j, l->get_b(i,j));
    }
}

void CHMM::invalidate_model()
{
	//initialize pat/mod_prob/alpha/beta cache as not calculated
	this->mod_prob=0.0;
	this->mod_prob_updated=false;
#ifndef NOVIT
	this->all_pat_prob=0.0;
	this->pat_prob=0.0;
	this->path_deriv_updated=false ;
	this->path_deriv_dimension=-1 ;
	this->all_path_prob_updated=false;
#endif  //NOVIT

	feature_cache_in_question=true;	

#ifdef PARALLEL
	{
	  for (int i=0; i<NUM_PARALLEL; i++)
	    {
	      this->alpha_cache[i].updated=false;
	      this->beta_cache[i].updated=false;
#ifndef NOVIT
	      path_prob_updated[i]=false ;
	      path_prob_dimension[i]=-1 ;
#endif // NOVIT
	    } ;
	} 
#else // PARALLEL
	this->alpha_cache.updated=false;
	this->beta_cache.updated=false;
#ifndef NOVIT
	this->path_prob_dimension=-1;
	this->path_prob_updated=false;
#endif //NOVIT

#endif // PARALLEL
}

void CHMM::open_bracket(FILE* file)
{
	int value;
	while (((value=fgetc(file)) != EOF) && (value!='['))	//skip possible spaces and end if '[' occurs
	{
		if (value=='\n')
			line++;
	}
	
	if (value==EOF)
		error(line, "expected \"[\" in input file");

	while (((value=fgetc(file)) != EOF) && (isspace(value)))	//skip possible spaces
	{
		if (value=='\n')
			line++;
	}

	ungetc(value, file);
}

void CHMM::close_bracket(FILE* file)
{
  int value;
  while (((value=fgetc(file)) != EOF) && (value!=']'))	//skip possible spaces and end if ']' occurs
    {
      if (value=='\n')
	line++;
    }
  
  if (value==EOF)
    error(line, "expected \"]\" in input file");
}

bool CHMM::comma_or_space(FILE* file)
{
  int value;
  while (((value=fgetc(file)) != EOF) && (value!=',') && (value!=';') && (value!=']'))	 //skip possible spaces and end if ',' or ';' occurs
    {
      if (value=='\n')
	line++;
    }
  if (value==']')
    {
      ungetc(value, file);
     CIO::message("found ']' instead of ';' or ','\n") ;
      return false ;
    } ;
  
  if (value==EOF)
    error(line, "expected \";\" or \",\" in input file");
  
  while (((value=fgetc(file)) != EOF) && (isspace(value)))	//skip possible spaces
    {
      if (value=='\n')
	line++;
    }
  ungetc(value, file);
  return true ;
}

bool CHMM::get_numbuffer(FILE* file, char* buffer, int length)
{
  signed char value;
  /*printf("par: len:%i\n",length) ;*/
  
  while (((value=fgetc(file)) != EOF) && 
	 !isdigit(value) && (value!='A') 
	 && (value!='C') && (value!='G') && (value!='T') 
	 && (value!='N') && (value!='n') 
	 && (value!='.') && (value!='-') && (value!='e') && (value!=']')) //skip possible spaces+crap
    {
      if (value=='\n')
	line++;
    }
  if (value==']')
    {
      ungetc(value,file) ;
      return false ;
    } ;
  if (value!=EOF)
    {
      int i=0;
      switch (value)
	{
	case 'A':
	  value='0' +B_A;
	  break;
	case 'C':
	  value='0' +B_C;
	  break;
	case 'G':
	  value='0' +B_G;
	  break;
	case 'T':
	  value='0' +B_T;
	  break;
	case 'N':
	  value='0' +B_N;
	  break;
	case 'n':
	  value='0' +B_n;
	  break;
	};
      
      buffer[i++]=value;
      
      while (((value=fgetc(file)) != EOF) && 
	     (isdigit(value) || (value=='.') || (value=='-') || (value=='e')
	      || (value=='A') || (value=='C') || (value=='G')|| (value=='T')
	      || (value=='N') || (value=='n')) && (i<length))
	{
	  /*printf("found : %i %c (pos:%li)\n",i,value,ftell(file)) ;*/
	  switch (value)
	    {
	    case 'A':
	      value='0' +B_A;
	      break;
	    case 'C':
	      value='0' +B_C;
	      break;
	    case 'G':
	      value='0' +B_G;
	      break;
	    case 'T':
	      value='0' +B_T;
	      break;
	    case 'N':
	      value='0' +B_N;
	      break;
	    case 'n':
	      value='0' +B_n;
	      break;
	    case '1': case '2': case'3': case '4': case'5':
	    case '6': case '7': case'8': case '9': case '0': break ;
	    case '.': case 'e': case '-': break ;
	    default:
	     CIO::message("found crap: %i %c (pos:%li)\n",i,value,ftell(file)) ;
	    };
	  buffer[i++]=value;
	}
      ungetc(value, file);
      buffer[i]='\0';
      
      return (i<=length) && (i>0); 
    }
  return false;
}

/*
	-format specs: model_file (model.hmm)
		% HMM - specification
		% N  - number of states
		% M  - number of observation_tokens
		% ORDER - order of hmm
		% a is state_transition_matrix 
		% size(a)= [N,N]
		%
		% b is observation_per_state_matrix
		% size(b)= [N,M] (for ORDER>1 M=realM^ORDER)
		%
		% p is initial distribution
		% size(p)= [1, N]

		N=<INT>;	
		M=<INT>;
		ORDER=<INT>;

		p=[<REAL>,<REAL>...<DOUBLE>];
		q=[<DOUBLE>,<DOUBLE>...<DOUBLE>];

		a=[ [<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
		  ];

		b=[ [<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
		  ];
*/

bool CHMM::load_model(FILE* file)
{
  unsigned int received_params=0x0000000;	//a,b,p,N,M,O
  
  bool result=false;
  E_STATE state=INITIAL;
  char buffer[1024];
  
  line=1;
  int i,j;
  
  if (file)
    {
      while (state!=END)
	{
	  int value=fgetc(file);
	  
	  if (value=='\n')
	    line++;
	  if (value==EOF)
	    state=END;
	  
	  switch (state)
	    {
	    case INITIAL:	// in the initial state only N,M,ORDER initialisations and comments are allowed
	      if (value=='N')
		{
		  if (received_params & GOTN)
		    error(line, "in model file: \"p double defined\"");
		  else
		    state=GET_N;
		}
	      else if (value=='M')
		{
		  if (received_params & GOTM)
		    error(line, "in model file: \"p double defined\"");
		  else
		    state=GET_M;
		}
	      else if (value=='O')
		{
		  if (fgetc(file)=='R' && fgetc(file)=='D' && fgetc(file)=='E' && fgetc(file)=='R')
		    {
		      if (received_params & GOTO)
			error(line, "in model file: \"p double defined\"");
		      else
			state=GET_ORDER;
		    }
		  else
		    error(line, "in model file: \"ORDER expected\"");
		}
	      else if (value=='%')
		{
		  state=COMMENT;
		}
	    case ARRAYs:	// when n,m, order are known p,a,b arrays are allowed to be read
	      if (value=='p')
		{
		  if (received_params & GOTp)
		    error(line, "in model file: \"p double defined\"");
		  else
		    state=GET_p;
		}
	      if (value=='q')
		{
		  if (received_params & GOTq)
		    error(line, "in model file: \"q double defined\"");
		  else
		    state=GET_q;
		}
	      else if (value=='a')
		{
		  if (received_params & GOTa)
		    error(line, "in model file: \"a double defined\"");
		  else
		    state=GET_a;
		}
	      else if (value=='b')
		{
		  if (received_params & GOTb)
		    error(line, "in model file: \"b double defined\"");
		  else
		    state=GET_b;
		}
	      else if (value=='%')
		{
		  state=COMMENT;
		}
	      break;
	    case GET_N:
	      if (value=='=')
		{
		  if (get_numbuffer(file, buffer, 4))	//get num
		    {
		      this->N= atoi(buffer);
		      received_params|=GOTN;
		      state= (received_params == (GOTN | GOTM | GOTO)) ? ARRAYs : INITIAL;
		    }
		  else
		    state=END;		//end if error
		}
	      break;
	    case GET_M:
	      if (value=='=')
		{
		  if (get_numbuffer(file, buffer, 4))	//get num
		    {
		      this->M= atoi(buffer);
		      received_params|=GOTM;
		      state= (received_params == (GOTN | GOTM | GOTO)) ? ARRAYs : INITIAL;
		    }
		  else
		    state=END;		//end if error
		}
	      break;
	    case GET_ORDER:
	      if (value=='=')
		{
		  if (get_numbuffer(file, buffer, 2))	//get num
		    {
		      this->ORDER= atoi(buffer);
		      
		      received_params|=GOTO;
		      state= (received_params == (GOTN | GOTM | GOTO)) ? ARRAYs : INITIAL;
		    }
		  else
		    state=END;		//end if error
		}
	      break;
	    case GET_a:
	      if (value=='=')
		{
		  double f;
		  
		  transition_matrix_a=new REAL[N*N];
		  open_bracket(file);
		  for (i=0; i<this->N; i++)
		    {
		      open_bracket(file);
		      
		      for (j=0; j<this->N ; j++)
			{
			  
			  if (fscanf( file, "%le", &f ) != 1)
			    error(line, "double expected");
			  else
			    set_a(i,j, f);
			  
			  if (j<this->N-1)
			    comma_or_space(file);
			  else
			    close_bracket(file);
			}
		      
		      if (i<this->N-1)
			comma_or_space(file);
		      else
			close_bracket(file);
		    }
		  received_params|=GOTa;
		}
	      state= (received_params == (GOTa | GOTb | GOTp | GOTq)) ? END : ARRAYs;
	      break;
	    case GET_b:
	      if (value=='=')
		{
		  double f;
		  
		  observation_matrix_b=new REAL[N*M];	
		  open_bracket(file);
		  for (i=0; i<this->N; i++)
		    {
		      open_bracket(file);
		      
		      for (j=0; j<this->M ; j++)
			{
			  
			  if (fscanf( file, "%le", &f ) != 1)
			    error(line, "double expected");
			  else
			    set_b(i,j, f);
			  
			  if (j<this->M-1)
			    comma_or_space(file);
			  else
			    close_bracket(file);
			}
		      
		      if (i<this->N-1)
			comma_or_space(file);
		      else
			close_bracket(file);
		    }	
		  received_params|=GOTb;
		}
	      state= ((received_params & (GOTa | GOTb | GOTp | GOTq)) == (GOTa | GOTb | GOTp | GOTq)) ? END : ARRAYs;
	      break;
	    case GET_p:
	      if (value=='=')
		{
		  double f;
		  
		  initial_state_distribution_p=new REAL[N];
		  open_bracket(file);
		  for (i=0; i<this->N ; i++)
		    {
		      if (fscanf( file, "%le", &f ) != 1)
			error(line, "double expected");
		      else
			set_p(i, f);
		      
		      if (i<this->N-1)
			comma_or_space(file);
		      else
			close_bracket(file);
		    }
		  received_params|=GOTp;
		}
	      state= (received_params == (GOTa | GOTb | GOTp | GOTq)) ? END : ARRAYs;
	      break;
	    case GET_q:
	      if (value=='=')
		{
		  double f;
		  
		  end_state_distribution_q=new REAL[N];
		  open_bracket(file);
		  for (i=0; i<this->N ; i++)
		    {
		      if (fscanf( file, "%le", &f ) != 1)
			error(line, "double expected");
		      else
			set_q(i, f);
		      
		      if (i<this->N-1)
			comma_or_space(file);
		      else
			close_bracket(file);
		    }
		  received_params|=GOTq;
		}
	      state= (received_params == (GOTa | GOTb | GOTp | GOTq)) ? END : ARRAYs;
	      break;
	    case COMMENT:
	      if (value==EOF)
		state=END;
	      else if (value=='\n')
		{
		  line++;
		  state=INITIAL;
		}
	      break;
	      
	    default:
	      break;
	    }
	}
      result= (received_params== (GOTa | GOTb | GOTp | GOTq | GOTN | GOTM | GOTO));
    }
 
  normalize(); 
  return result;
}

/*	
	-format specs: train_file (train.trn)
		% HMM-TRAIN - specification
		% learn_a - elements in state_transition_matrix to be learned
		% learn_b - elements in oberservation_per_state_matrix to be learned
		%			note: each line stands for 
		%				<state>, <observation(-ORDER)>, observation(-ORDER+1)...observation(NOW)>
		% learn_p - elements in initial distribution to be learned
		% learn_q - elements in the end-state distribution to be learned
		%
		% const_x - specifies initial values of elements
		%				rest is assumed to be 0.0
		%
		%	NOTE: IMPLICIT DEFINES:
		%		#define A 0
		%		#define C 1
		%		#define G 2
		%		#define T 3
		%

		learn_a=[ [<INT>,<INT>]; 
				  [<INT>,<INT>]; 
				  [<INT>,<INT>]; 
					........
				  [<INT>,<INT>]; 
				  [-1,-1];
				];

		learn_b=[ [<INT>,<INT>,<INT>,...,<INT>]; 
				  [<INT>,<INT>,<INT>,...,<INT>]; 
				  [<INT>,<INT>,<INT>,...,<INT>]; 
					........
				  [<INT>,<INT>,<INT>,...,<INT>]; 
				  [-1,-1];
				];

		learn_p= [ <INT>, ... , <INT>, -1 ];
		learn_q= [ <INT>, ... , <INT>, -1 ];


		const_a=[ [<INT>,<INT>,<DOUBLE>]; 
					  [<INT>,<INT>,<DOUBLE>]; 
					  [<INT>,<INT>,<DOUBLE>]; 
						........
					  [<INT>,<INT>,<DOUBLE>]; 
					  [-1,-1,-1];
					];

		const_b=[ [<INT>,<INT>,<INT>,...,<INT>,<DOUBLE>]; 
				  [<INT>,<INT>,<INT>,...,<INT>,<DOUBLE>]; 
				  [<INT>,<INT>,<INT>,...,<INT>,<DOUBLE]; 
					........
				  [<INT>,<INT>,<INT>,...,<INT>,<DOUBLE>]; 
				  [-1,-1];
				];

		const_p[]=[ [<INT>, <DOUBLE>], ... , [<INT>,<DOUBLE>], [-1,-1] ];
		const_q[]=[ [<INT>, <DOUBLE>], ... , [<INT>,<DOUBLE>], [-1,-1] ];
*/
bool CHMM::load_definitions(FILE* file, bool verbose, bool initialize)
{
  if (model)
    delete model ;
  model=new CModel();
  
  unsigned char received_params=0x0000000;	//a,b,p,q,N,M,O
  char buffer[1024];
  
  bool result=false;
  E_STATE state=INITIAL;
  
  { // do some useful initializations 
    model->set_learn_a(0, -1);
    model->set_learn_a(1, -1);
    model->set_const_a(0, -1);
    model->set_const_a(1, -1);
    model->set_const_a_val(0, 1.0);
    for (int i=0; i<ORDER+1; i++) 
      {
	model->set_learn_b(i, -1);
	model->set_const_b(i, -1);
	model->set_const_b_val(i, 1.0);
      }
    model->set_learn_p(0, -1);
    model->set_learn_q(0, -1);
    model->set_const_p(0, -1);
    model->set_const_q(0, -1);
  } ;
  
  line=1;
  
  if (file)
    {
      while (state!=END)
	{
	  int value=fgetc(file);
	  
	  if (value=='\n')
	    line++;
	  
	  if (value==EOF)
	    state=END;
	  
	  switch (state)
	    {
	    case INITIAL:	
	      if (value=='l')
		{
		  if (fgetc(file)=='e' && fgetc(file)=='a' && fgetc(file)=='r' && fgetc(file)=='n' && fgetc(file)=='_')
		    {
		      switch(fgetc(file))
			{
			case 'a':
			  state=GET_learn_a;
			  break;
			case 'b':
			  state=GET_learn_b;
			  break;
			case 'p':
			  state=GET_learn_p;
			  break;
			case 'q':
			  state=GET_learn_q;
			  break;
			default:
			  error(line, "a,b,p or q expected in train definition file");
			};
		    }
		}
	      else if (value=='c')
		{
		  if (fgetc(file)=='o' && fgetc(file)=='n' && fgetc(file)=='s' 
		      && fgetc(file)=='t' && fgetc(file)=='_')
		    {
		      switch(fgetc(file))
			{
			case 'a':
			  state=GET_const_a;
			  break;
			case 'b':
			  state=GET_const_b;
			  break;
			case 'p':
			  state=GET_const_p;
			  break;
			case 'q':
			  state=GET_const_q;
			  break;
			default:
			  error(line, "a,b,p or q expected in train definition file");
			};
		    }
		}
	      else if (value=='%')
		{
		  state=COMMENT;
		}
	      else if (value==EOF)
		{
		  state=END;
		}
	      break;
	    case GET_learn_a:
	      if (value=='=')
		{
		  open_bracket(file);
		  bool finished=false;
		  int i=0;
		  
		  if (verbose) 
		   CIO::message("\nlearn for transition matrix: ") ;
		  while (!finished)
		    {
		      open_bracket(file);
		      
		      if (get_numbuffer(file, buffer, 4))	//get num
			{
			  value=atoi(buffer);
			  model->set_learn_a(i++, value);
			  
			  if (value<0)
			    {
			      finished=true;
			      break;
			    }
			  if (value>=N)
			   CIO::message("invalid value for learn_a(%i,0): %i\n",i/2,(int)value) ;
			}
		      else
			break;
		      
		      //if (verbose)
		      // CIO::message("(%i,",value) ;
		      
		      comma_or_space(file);
		      
		      if (get_numbuffer(file, buffer, 4))	//get num
			{
			  value=atoi(buffer);
			  model->set_learn_a(i++, value);
			  
			  if (value<0)
			    {
			      finished=true;
			      break;
			    }
			  if (value>=N)
			   CIO::message("invalid value for learn_a(%i,1): %i\n",i/2-1,(int)value) ;
			  
			}
		      else
			break;
		      close_bracket(file);
		      //if (verbose)
		      // CIO::message("%i)",(int)value) ;
		    }
		  close_bracket(file);
		  if (verbose) 
		   CIO::message("%i Entries",(int)(i/2)) ;
		  
		  if (finished)
		    {
		      received_params|=GOTlearn_a;
		      state= (received_params == (GOTlearn_a | GOTlearn_b | GOTlearn_p | GOTlearn_q |GOTconst_a | GOTconst_b | GOTconst_p | GOTconst_q)) ? END : INITIAL;
		    }
		  else
		    state=END;
		}
	      break;
	    case GET_learn_b:
	      if (value=='=')
		{
		  open_bracket(file);
		  bool finished=false;
		  int i=0;
		  
		  if (verbose) 
		   CIO::message("\nlearn for emission matrix:   ") ;
		  while (!finished)
		    {
		      open_bracket(file);
		      
		      int combine=0;
		      
		      for (int j=0; j<ORDER+1; j++)
			{
			  if (get_numbuffer(file, buffer, 4))	//get num
			    {
			      value=atoi(buffer);
			      
			      if (j==0)
				{
				  model->set_learn_b(i++, value);
				  
				  if (value<0)
				    {
				      finished=true;
				      break;
				    }
				  if (value>=N)
				   CIO::message("invalid value for learn_b(%i,0): %i\n",i/2,(int)value) ;
				}
			      else 
				combine= (combine << MAX_M) | value;
			    }
			  else
			    break;
			  
			  if (j<ORDER)
			    comma_or_space(file);
			  else
			    close_bracket(file);
			}
		      model->set_learn_b(i++, combine);
		      if (combine>=M)
			printf("invalid value for learn_b(%i,1): %i\n",i/2-1,(int)value) ;
		    }
		  close_bracket(file);
		  if (verbose) 
		   CIO::message("%i Entries",(int)(i/2-1)) ;
		  
		  if (finished)
		    {
		      received_params|=GOTlearn_b;
		      state= (received_params == (GOTlearn_a | GOTlearn_b | GOTlearn_p | GOTlearn_q |GOTconst_a | GOTconst_b | GOTconst_p | GOTconst_q)) ? END : INITIAL;
		    }
		  else
		    state=END;
		}
	      break;
	    case GET_learn_p:
	      if (value=='=')
		{
		  open_bracket(file);
		  bool finished=false;
		  int i=0;
		  
		  if (verbose) 
		   CIO::message("\nlearn start states: ") ;
		  while (!finished)
		    {
		      if (get_numbuffer(file, buffer, 4))	//get num
			{
			  value=atoi(buffer);

			  model->set_learn_p(i++, value);
			  
			  if (value<0)
			    {
			      finished=true;
			      break;
			    }
			  if (value>=N)
			   CIO::message("invalid value for learn_p(%i): %i\n",i-1,(int)value) ;
			}
		      else
			break;
		      
		      comma_or_space(file);
		    }
		  
		  close_bracket(file);
		  if (verbose) 
		   CIO::message("%i Entries",i-1) ;
		  
		  if (finished)
		    {
		      received_params|=GOTlearn_p;
		      state= (received_params == (GOTlearn_a | GOTlearn_b | GOTlearn_p | GOTlearn_q |GOTconst_a | GOTconst_b | GOTconst_p | GOTconst_q)) ? END : INITIAL;
		    }
		  else
		    state=END;
		}
	      break;
	    case GET_learn_q:
	      if (value=='=')
		{
		  open_bracket(file);
		  bool finished=false;
		  int i=0;
		  
		  if (verbose) 
		   CIO::message("\nlearn terminal states: ") ;
		  while (!finished)
		    {
		      if (get_numbuffer(file, buffer, 4))	//get num
			{
			  value=atoi(buffer);
			  model->set_learn_q(i++, value);
			  
			  if (value<0)
			    {
			      finished=true;
			      break;
			    }
			  if (value>=N)
			   CIO::message("invalid value for learn_q(%i): %i\n",i-1,(int)value) ;
			}
		      else
			break;
		      
		      comma_or_space(file);
		    }
		  
		  close_bracket(file);
		  if (verbose) 
		   CIO::message("%i Entries",i-1) ;
		  
		  if (finished)
		    {
		      received_params|=GOTlearn_q;
		      state= (received_params == (GOTlearn_a | GOTlearn_b | GOTlearn_p | GOTlearn_q |GOTconst_a | GOTconst_b | GOTconst_p | GOTconst_q)) ? END : INITIAL;
		    }
		  else
		    state=END;
		}
	      break;
	    case GET_const_a:
	      if (value=='=')
		{
		  open_bracket(file);
		  bool finished=false;
		  int i=0;
		  
		  if (verbose) 
#ifdef DEBUG
		   CIO::message("\nconst for transition matrix: \n") ;
#else
		 CIO::message("\nconst for transition matrix: ") ;
#endif
		  while (!finished)
		    {
		      open_bracket(file);
		      
		      if (get_numbuffer(file, buffer, 4))	//get num
			{
			  value=atoi(buffer);
			  model->set_const_a(i++, value);
			  
			  if (value<0)
			    {
			      finished=true;
			      model->set_const_a(i++, value);
			      model->set_const_a_val((int)i/2 - 1, value);
			      break;
			    }
			  if (value>=N)
			   CIO::message("invalid value for const_a(%i,0): %i\n",i/2,(int)value) ;
			}
		      else
			break;
		      
		      comma_or_space(file);
		      
		      if (get_numbuffer(file, buffer, 4))	//get num
			{
			  value=atoi(buffer);
			  model->set_const_a(i++, value);
			  
			  if (value<0)
			    {
			      finished=true;
			      model->set_const_a_val((int)i/2 - 1, value);
			      break;
			    }
			  if (value>=N)
			   CIO::message("invalid value for const_a(%i,1): %i\n",i/2-1,(int)value) ;
			}
		      else
			break;
		      
		      if (!comma_or_space(file))
			model->set_const_a_val((int)i/2 - 1, 1.0);
		      else
			if (get_numbuffer(file, buffer, 10))	//get num
			  {
			    REAL dvalue=atof(buffer);
			    model->set_const_a_val((int)i/2 - 1, dvalue);
			    if (dvalue<0)
			      {
				finished=true;
				break;
			      }
			    if ((dvalue>1.0) || (dvalue<0.0))
			     CIO::message("invalid value for const_a_val(%i): %e\n",(int)i/2-1,dvalue) ;
			  }
			else
			  model->set_const_a_val((int)i/2 - 1, 1.0);
		      
#ifdef DEBUG
		      if (verbose)
			printf("const_a(%i,%i)=%e\n", model->get_const_a((int)i/2-1,0),model->get_const_a((int)i/2-1,1),model->get_const_a_val((int)i/2-1)) ;
#endif
		      close_bracket(file);
		    }
		  close_bracket(file);
		  if (verbose) 
		   CIO::message("%i Entries",(int)i/2-1) ;
		  
		  if (finished)
		    {
		      received_params|=GOTconst_a;
		      state= (received_params == (GOTlearn_a | GOTlearn_b | GOTlearn_p | GOTlearn_q |GOTconst_a | GOTconst_b | GOTconst_p | GOTconst_q)) ? END : INITIAL;
		    }
		  else
		    state=END;
		}
	      break;
	    case GET_const_b:
	      if (value=='=')
		{
		  open_bracket(file);
		  bool finished=false;
		  int i=0;
		  
		  if (verbose) 
#ifdef DEBUG
		   CIO::message("\nconst for emission matrix:   \n") ;
#else
		 CIO::message("\nconst for emission matrix:   ") ;
#endif
		  while (!finished)
		    {
		      open_bracket(file);
		      int combine=0;
		      for (int j=0; j<ORDER+2; j++)
			{
			  if (get_numbuffer(file, buffer, 10))	//get num
			    {
			      //printf("j=%i; buffer='%s'\n",j, buffer) ;
			      if (j==0)
				{
				  value=atoi(buffer);
				  
				  model->set_const_b(i++, value);
				  
				  if (value<0)
				    {
				      finished=true;
				      //model->set_const_b_val((int)(i-1)/2, value);
				      break;
				    }
				  if (value>=N)
				   CIO::message("invalid value for const_b(%i,0): %i\n",i/2-1,(int)value) ;
				}
			      else if (j==ORDER+1)
				{
				  REAL dvalue=atof(buffer);
				  model->set_const_b_val((int)(i-1)/2, dvalue);
				  if (dvalue<0)
				    {
				      finished=true;
				      break;
				    } ;
				  if ((dvalue>1.0) || (dvalue<0.0))
				   CIO::message("invalid value for const_b_val(%i,1): %e\n",i/2-1,dvalue) ;
				}
			      else
				{
				  value=atoi(buffer);
				  combine= (combine << MAX_M) | value;
				} ;
			    }
			  else
			    {
			      if (j==ORDER+1)
				model->set_const_b_val((int)(i-1)/2, 1.0);
			      break;
			    } ;
			  if (j<ORDER+1)
			    if ((!comma_or_space(file)) && (j==ORDER))
			      {
				model->set_const_b_val((int)(i-1)/2, 1.0) ;
				break ;
			      } ;
			}
		      close_bracket(file);
		      model->set_const_b(i++, combine);
		      if (combine>=M)
			printf("invalid value for const_b(%i,1): %i\n",i/2-1, combine) ;
#ifdef DEBUG
		      if (verbose && !finished)
			printf("const_b(%i,%i)=%e\n", model->get_const_b((int)i/2-1,0),model->get_const_b((int)i/2-1,1),model->get_const_b_val((int)i/2-1)) ;
#endif
		    }
		  close_bracket(file);
		  if (verbose) 
		   CIO::message("%i Entries",(int)i/2-1) ;
		  
		  if (finished)
		    {
		      received_params|=GOTconst_b;
		      state= (received_params == (GOTlearn_a | GOTlearn_b | GOTlearn_p | GOTlearn_q |GOTconst_a | GOTconst_b | GOTconst_p | GOTconst_q)) ? END : INITIAL;
		    }
		  else
		    state=END;
		}
	      break;
	    case GET_const_p:
	      if (value=='=')
		{
		  open_bracket(file);
		  bool finished=false;
		  int i=0;
		  
		  if (verbose) 
#ifdef DEBUG
		   CIO::message("\nconst for start states:     \n") ;
#else
		 CIO::message("\nconst for start states:     ") ;
#endif
		  while (!finished)
		    {
		      open_bracket(file);
		      
		      if (get_numbuffer(file, buffer, 4))	//get num
			{
			  value=atoi(buffer);
			  model->set_const_p(i, value);
			  
			  if (value<0)
			    {
			      finished=true;
			      model->set_const_p_val(i++, value);
			      break;
			    }
			  if (value>=N)
			   CIO::message("invalid value for const_p(%i): %i\n",i,(int)value) ;

			}
		      else
			break;
		      
		      if (!comma_or_space(file))
			model->set_const_p_val(i++, 1.0);
		      else
			if (get_numbuffer(file, buffer, 10))	//get num
			  {
			    REAL dvalue=atof(buffer);
			    model->set_const_p_val(i++, dvalue);
			    if (dvalue<0)
			      {
				finished=true;
				break;
			      }
			    if ((dvalue>1) || (dvalue<0))
			     CIO::message("invalid value for const_p_val(%i): %e\n",i,dvalue) ;
			  }
			else
			  model->set_const_p_val(i++, 1.0);
		      
		      close_bracket(file);
		      
#ifdef DEBUG
		      if (verbose)
			printf("const_p(%i)=%e\n", model->get_const_p(i-1),model->get_const_p_val(i-1)) ;
#endif
		    }
		  if (verbose) 
		   CIO::message("%i Entries",i-1) ;
		  
		  close_bracket(file);
		  
		  if (finished)
		    {
		      received_params|=GOTconst_p;
		      state= (received_params == (GOTlearn_a | GOTlearn_b | GOTlearn_p | GOTlearn_q |GOTconst_a | GOTconst_b | GOTconst_p | GOTconst_q)) ? END : INITIAL;
		    }
		  else
		    state=END;
		}
	      break;
	    case GET_const_q:
	      if (value=='=')
		{
		  open_bracket(file);
		  bool finished=false;
		  if (verbose) 
#ifdef DEBUG
		   CIO::message("\nconst for terminal states: \n") ;
#else
		 CIO::message("\nconst for terminal states: ") ;
#endif
		  int i=0;
		  
		  while (!finished)
		    {
		      open_bracket(file) ;
		      if (get_numbuffer(file, buffer, 4))	//get num
			{
			  value=atoi(buffer);
			  model->set_const_q(i, value);
			  if (value<0)
			    {
			      finished=true;
			      model->set_const_q_val(i++, value);
			      break;
			    }
			  if (value>=N)
			   CIO::message("invalid value for const_q(%i): %i\n",i,(int)value) ;
			}
		      else
			break;
		      
		      if (!comma_or_space(file))
			model->set_const_q_val(i++, 1.0);
		      else
			if (get_numbuffer(file, buffer, 10))	//get num
			  {
			    REAL dvalue=atof(buffer);
			    model->set_const_q_val(i++, dvalue);
			    if (dvalue<0)
			      {
				finished=true;
				break;
			      }
			    if ((dvalue>1) || (dvalue<0))
				printf("invalid value for const_q_val(%i): %e\n",i,(double) dvalue) ;
			  }
			else
			  model->set_const_q_val(i++, 1.0);
		      
		      close_bracket(file);
#ifdef DEBUG
		      if (verbose)
			printf("const_q(%i)=%e\n", model->get_const_q(i-1),model->get_const_q_val(i-1)) ;
#endif
		    }
		  if (verbose)
		   CIO::message("%i Entries",i-1) ;
		  
		  close_bracket(file);
		  
		  if (finished)
		    {
		      received_params|=GOTconst_q;
		      state= (received_params == (GOTlearn_a | GOTlearn_b | GOTlearn_p | GOTlearn_q |GOTconst_a | GOTconst_b | GOTconst_p | GOTconst_q)) ? END : INITIAL;
		    }
		  else
		    state=END;
		}
	      break;
	    case COMMENT:
	      if (value==EOF)
		state=END;
	      else if (value=='\n')
		state=INITIAL;
	      break;
	      
	    default:
	      break;
	    }
	}
    }
  
  /*result=((received_params&(GOTlearn_a | GOTconst_a))!=0) ; 
    result=result && ((received_params&(GOTlearn_b | GOTconst_b))!=0) ; 
    result=result && ((received_params&(GOTlearn_p | GOTconst_p))!=0) ; 
    result=result && ((received_params&(GOTlearn_q | GOTconst_q))!=0) ; */
  result=1 ;
  if (result)
    {
      model->sort_learn_a() ;
      model->sort_learn_b() ;
      if (initialize)
	{
	  init_model_defined(); ;
	  convert_to_log();
	} ;
    }
  if (verbose)
   CIO::message("\n") ;
  return result;
}

/*
	-format specs: model_file (model.hmm)
		% HMM - specification
		% N  - number of states
		% M  - number of observation_tokens
		% ORDER - order of hmm
		% a is state_transition_matrix 
		% size(a)= [N,N]
		%
		% b is observation_per_state_matrix
		% size(b)= [N,M^ORDER]
		%
		% p is initial distribution
		% size(p)= [1, N]

		N=<INT>;	
		M=<INT>;
		ORDER=<INT>;

		p=[<DOUBLE>,<DOUBLE>...<DOUBLE>];

		a=[ [<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
		  ];

		b=[ [<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
			[<DOUBLE>,<DOUBLE>...<DOUBLE>];
		  ];
*/

bool CHMM::save_model(FILE* file)
{
	bool result=false;
	int i,j;
	const float NAN_REPLACEMENT = (float) CMath::ALMOST_NEG_INFTY ;

	if (file)
	  {
	    fprintf(file,"%s","% HMM - specification\n% N  - number of states\n% M  - number of observation_tokens\n% ORDER - order of hmm (has to be constant ONE this time, high order models can be mapped to M anyway)\n% a is state_transition_matrix\n% size(a)= [N,N]\n%\n% b is observation_per_state_matrix\n% size(b)= [N,M]\n%\n% p is initial distribution\n% size(p)= [1, N]\n\n% q is distribution of end states\n% size(q)= [1, N]\n");
	    fprintf(file,"N=%d;\n",N);
	    fprintf(file,"M=%d;\n",M);
	    fprintf(file,"ORDER=%d;\n\n",ORDER);
	    
	    fprintf(file,"p=[");
	    for (i=0; i<N; i++)
	      {
		if (i<N-1) {
		  if (finite(get_p(i)))
		    fprintf(file, "%e,", (double)get_p(i));
		  else
		    fprintf(file, "%f,", NAN_REPLACEMENT);			
		}
		else {
		  if (finite(get_p(i)))
		    fprintf(file, "%e", (double)get_p(i));
		  else
		    fprintf(file, "%f", NAN_REPLACEMENT);
		}
	      }

		fprintf(file,"];\n\nq=[");
	    for (i=0; i<N; i++)
	      {
		if (i<N-1) {
		  if (finite(get_q(i)))
		    fprintf(file, "%e,", (double)get_q(i));
		  else
		    fprintf(file, "%f,", NAN_REPLACEMENT);			
		}
		else {
		  if (finite(get_q(i)))
		    fprintf(file, "%e", (double)get_q(i));
		  else
		    fprintf(file, "%f", NAN_REPLACEMENT);
		}
	      }
	    fprintf(file,"];\n\na=[");
	    
	    for (i=0; i<N; i++)
	      {
		fprintf(file, "\t[");
		
		for (j=0; j<N; j++)
		  {
		    if (j<N-1) {
		      if (finite(get_a(i,j)))
			fprintf(file, "%e,", (double)get_a(i,j));
		      else
			fprintf(file, "%f,", NAN_REPLACEMENT);
		    }
		    else {
		      if (finite(get_a(i,j)))
			fprintf(file, "%e];\n", (double)get_a(i,j));
		      else
			fprintf(file, "%f];\n", NAN_REPLACEMENT);
		    }
		  }
	      }
	    
	    fprintf(file,"  ];\n\nb=[");
	    
	    for (i=0; i<N; i++)
	      {
		fprintf(file, "\t[");
		
		for (j=0; j<M; j++)
		  {
		    if (j<M-1) {
		      if (finite(get_b(i,j)))
			fprintf(file, "%e,",  (double)get_b(i,j));
		      else
			fprintf(file, "%f,", NAN_REPLACEMENT);
		    }
		    else {
		      if (finite(get_b(i,j)))
			fprintf(file, "%e];\n", (double)get_b(i,j));
		      else
			fprintf(file, "%f];\n", NAN_REPLACEMENT);
		    }
		  }
		}
	    result= (fprintf(file,"  ];\n") == 5);
	  }
	
	return result;
}

#ifndef NOVIT
bool CHMM::save_path(FILE* file)
{
	bool result=false;

	if (file)
	{
		best_path(0);
		//fprintf(file,"path probability:%e\n\nstate sequence:\n", prob);
		printf("writing %i states...", p_observations->get_obs_T(0)) ;
		for (int i=0; i<p_observations->get_obs_T(0)-1; i++)
			fprintf(file,"%d ", PATH(0)[i]);

		fprintf(file,"%d", PATH(0)[p_observations->get_obs_T(0)-1]);
		printf("done\n") ;
	}

	return result;
}
#endif // NOVIT

bool CHMM::save_likelihood_bin(FILE* file)
{
	bool result=false;

	if (file)
	{
	  for (int dim=0; dim<p_observations->get_DIMENSION(); dim++)
	  {
	      float prob= (float) model_probability(dim);
	      fwrite(&prob, sizeof(float), 1, file);
	  }
	  result=true;
	}

	return result;
}

bool CHMM::save_likelihood(FILE* file)
{
	bool result=false;

	if (file)
	{
	  fprintf(file, "%% likelihood of model per observation\n%% P[O|model]=[ P[O|model]_1 P[O|model]_2 ... P[O|model]_dim ]\n%%\n");

	  fprintf(file, "P=[");
	  for (int dim=0; dim<p_observations->get_DIMENSION(); dim++)
	    fprintf(file, "%e ", (double) model_probability(dim));
	  
	  fprintf(file,"];");
	  result=true;
	}

	return result;
}

#define FLOATWRITE(file, value) { float rrr=float(value); fwrite(&rrr, sizeof(float), 1, file); num_floats++;}

bool CHMM::save_model_bin(FILE* file)
{
  int i,j,q, num_floats=0 ;
  if (!model)
    {
      if (file)
	{
	  // write id
	  FLOATWRITE(file, (float)CMath::INFTY);	  
	  FLOATWRITE(file, (float) 1);	  
	  
	  //derivates log(dp),log(dq)
	  for (i=0; i<N; i++)
	    FLOATWRITE(file, get_p(i));	  
	 CIO::message("wrote %i parameters for p\n",N) ;
	  
	  for (i=0; i<N; i++)
	    FLOATWRITE(file, get_q(i)) ;   
	 CIO::message("wrote %i parameters for q\n",N) ;
	  
	  //derivates log(da),log(db)
	  for (i=0; i<N; i++)
	    for (j=0; j<N; j++)
	      FLOATWRITE(file, get_a(i,j));
	 CIO::message("wrote %i parameters for a\n",N*N) ;
	  
	  for (i=0; i<N; i++)
	    for (j=0; j<M; j++)
	      FLOATWRITE(file, get_b(i,j));
	 CIO::message("wrote %i parameters for b\n",N*M) ;

	  // write id
	  FLOATWRITE(file, (float)CMath::INFTY);	  
	  FLOATWRITE(file, (float) 3);	  

	  // write number of parameters
	  FLOATWRITE(file, (float) N);	  
	  FLOATWRITE(file, (float) N);	  
	  FLOATWRITE(file, (float) N*N);	  
	  FLOATWRITE(file, (float) N*M);	  
	  FLOATWRITE(file, (float) N);	  
	  FLOATWRITE(file, (float) M);	  
	} ;
    } 
  else
    {
      if (file)
	{
	  int num_p, num_q, num_a, num_b ;
	  // write id
	  FLOATWRITE(file, (float)CMath::INFTY);	  
	  FLOATWRITE(file, (float) 2);	  
	  
	  for (i=0; model->get_learn_p(i)>=0; i++)
	    FLOATWRITE(file, get_p(model->get_learn_p(i)));	  
	  num_p=i ;
	 CIO::message("wrote %i parameters for p\n",num_p) ;
	  
	  for (i=0; model->get_learn_q(i)>=0; i++)
	    FLOATWRITE(file, get_q(model->get_learn_q(i)));    
	  num_q=i ;
	 CIO::message("wrote %i parameters for q\n",num_q) ;
	  
	  //derivates log(da),log(db)
	  for (q=0; model->get_learn_a(q,1)>=0; q++)
	    {
	      i=model->get_learn_a(q,0) ;
	      j=model->get_learn_a(q,1) ;
	      FLOATWRITE(file, (float)i);
	      FLOATWRITE(file, (float)j);
	      FLOATWRITE(file, get_a(i,j));
	    } ;
	  num_a=q ;
	 CIO::message("wrote %i parameters for a\n",num_a) ;		  
	  
	  for (q=0; model->get_learn_b(q,0)>=0; q++)
	    {
	      i=model->get_learn_b(q,0) ;
	      j=model->get_learn_b(q,1) ;
	      FLOATWRITE(file, (float)i);
	      FLOATWRITE(file, (float)j);
	      FLOATWRITE(file, get_b(i,j));
	    } ;
	  num_b=q ;
	 CIO::message("wrote %i parameters for b\n",num_b) ;

	  // write id
	  FLOATWRITE(file, (float)CMath::INFTY);	  
	  FLOATWRITE(file, (float) 3);	  

	  // write number of parameters
	  FLOATWRITE(file, (float) num_p);	  
	  FLOATWRITE(file, (float) num_q);	  
	  FLOATWRITE(file, (float) num_a);	  
	  FLOATWRITE(file, (float) num_b);	  
	  FLOATWRITE(file, (float) N);	  
	  FLOATWRITE(file, (float) M);	  
	} ;
    } ;
  return true ;
}

#ifndef NOVIT
bool CHMM::save_path_derivatives(FILE* logfile)
{
	int dim,i,j;
	REAL prob;

	if (logfile)
	  {
	    fprintf(logfile,"%% lambda denotes the model\n%% O denotes the observation sequence\n%% Q denotes the path\n%% \n%% calculating derivatives of P[O,Q|lambda]=p_{Q1}b_{Q1}(O_1}*a_{Q1}{Q2}b_{Q2}(O2)*...*q_{T-1}{T}b_{QT}(O_T}q_{Q_T} against p,q,a,b\n%%\n");
	    fprintf(logfile,"%% dPr[...]=[ [dp_1,...,dp_N,dq_1,...,dq_N, da_11,da_12,..,da_1N,..,da_NN, db_11,.., db_NN]\n");
	    fprintf(logfile,"%%            [dp_1,...,dp_N,dq_1,...,dq_N, da_11,da_12,..,da_1N,..,da_NN, db_11,.., db_NN]\n");
	    fprintf(logfile,"%%                            .............................                                \n");
	    fprintf(logfile,"%%            [dp_1,...,dp_N,dq_1,...,dq_N, da_11,da_12,..,da_1N,..,da_NN, db_11,.., db_MM]\n");
	    fprintf(logfile,"%%          ];\n%%\n\ndPr(log()) = [\n");
	  }
	else
	  return false ;
	
	for (dim=0; dim<p_observations->get_DIMENSION(); dim++)
	  {	
	    prob=best_path(dim);
	    
	    fprintf(logfile, "[ ");
	    
				//derivates dlogp,dlogq
	    for (i=0; i<N; i++)
	      fprintf(logfile,"%e, ", (double) path_derivative_p(i,dim) );
	    
	    for (i=0; i<N; i++)
	      fprintf(logfile,"%e, ", (double) path_derivative_q(i,dim) );
	    
				//derivates dloga,dlogb
	    for (i=0; i<N; i++)
	      for (j=0; j<N; j++)
		fprintf(logfile, "%e,", (double) path_derivative_a(i,j,dim) );
	    
	    for (i=0; i<N; i++)
	      for (j=0; j<M; j++)
		fprintf(logfile, "%e,", (double) path_derivative_b(i,j,dim) );
	    
	    fseek(logfile,ftell(logfile)-1,SEEK_SET);
	    fprintf(logfile, " ];\n");
	  }
	
	fprintf(logfile, "];");
	
	return true ;
}

bool CHMM::save_path_derivatives_bin(FILE* logfile)
{
  bool result=false;
  int dim,i,j,q;
  REAL prob=0 ;
  int num_floats=0 ;
  
  REAL sum_prob=0.0 ;
  if (!model)
   CIO::message("No definitions loaded -- writing derivatives of all weights\n") ;
  else
   CIO::message("writing derivatives of changed weights only\n") ;
  
  for (dim=0; dim<p_observations->get_DIMENSION(); dim++)
    {		      
      if (dim%100==0)
	{
	 CIO::message(".") ; 
	  
	} ;
      
      prob=best_path(dim);
      sum_prob+=prob ;
      
      if (!model)
	{
	  if (logfile)
	    {
	      // write prob
	      FLOATWRITE(logfile, prob);	  
	      
	      for (i=0; i<N; i++)
		FLOATWRITE(logfile, path_derivative_p(i,dim));
	      
	      for (i=0; i<N; i++)
		FLOATWRITE(logfile, path_derivative_q(i,dim));
	      
	      for (i=0; i<N; i++)
		for (j=0; j<N; j++)
		  FLOATWRITE(logfile, path_derivative_a(i,j,dim));
	      
	      for (i=0; i<N; i++)
		for (j=0; j<M; j++)
		  FLOATWRITE(logfile, path_derivative_b(i,j,dim));
	      
	    }
	} 
      else
	{
	  if (logfile)
	    {
	      // write prob
	      FLOATWRITE(logfile, prob);	  
	      
	      for (i=0; model->get_learn_p(i)>=0; i++)
		FLOATWRITE(logfile, path_derivative_p(model->get_learn_p(i),dim));
	      
	      for (i=0; model->get_learn_q(i)>=0; i++)
		FLOATWRITE(logfile, path_derivative_q(model->get_learn_q(i),dim));
	      
	      for (q=0; model->get_learn_a(q,0)>=0; q++)
		{
		  i=model->get_learn_a(q,0) ;
		  j=model->get_learn_a(q,1) ;
		  FLOATWRITE(logfile, path_derivative_a(i,j,dim));
		} ;
	      
	      for (q=0; model->get_learn_b(q,0)>=0; q++)
		{
		  i=model->get_learn_b(q,0) ;
		  j=model->get_learn_b(q,1) ;
		  FLOATWRITE(logfile, path_derivative_b(i,j,dim));
		} ;
	    }
	} ;      
    }
  save_model_bin(logfile) ;
  
  result=true;
 CIO::message("\n") ;
  return result;
}
#endif // NOVIT

bool CHMM::save_model_derivatives_bin(FILE* file)
{
  bool result=false;
  int dim,i,j,q ;
  int num_floats=0 ;
  
  if (!model)
   CIO::message("No definitions loaded -- writing derivatives of all weights\n") ;
  else
   CIO::message("writing derivatives of changed weights only\n") ;
  
#ifdef PARALLEL
  pthread_t *threads=new pthread_t[NUM_PARALLEL] ;
  S_THREAD_PARAM *params=new S_THREAD_PARAM[NUM_PARALLEL] ;
#endif
  
  for (dim=0; dim<p_observations->get_DIMENSION(); dim++)
    {		      
      if (dim%20==0)
	{
	 CIO::message(".") ; 
	  
	} ;
      
#ifdef PARALLEL
      if (dim%NUM_PARALLEL==0)
	{
	  int i ;
	  for (i=0; i<NUM_PARALLEL; i++)
	    if (dim+i<p_observations->get_DIMENSION())
	      {
		/*CIO::message(stderr,"creating thread for dim=%i\n",dim+i) ;*/
		
		params[i].hmm=this ;
		params[i].dim=dim+i ;
#ifdef SUNOS
		thr_create(NULL,0,bw_dim_prefetch, (void*)&params[i], PTHREAD_SCOPE_SYSTEM, &threads[i]) ;
#else // SUNOS
		pthread_create(&threads[i], NULL, bw_dim_prefetch, (void*)&params[i]) ;
#endif // SUNOS
	      } ;
	  for (i=0; i<NUM_PARALLEL; i++)
	    if (dim+i<p_observations->get_DIMENSION())
	      {
		void * ret ;
		pthread_join(threads[i], &ret) ;
		/*CIO::message(stderr,"thread for dim=%i returned: %i\n",dim+i,ALPHA_CACHE(dim+i).dimension) ;*/
	      } ;
	} ;
#endif
      
      REAL prob=model_probability(dim) ;
      if (!model)
	{
	  if (file)
	    {
	      // write prob
	      FLOATWRITE(file, prob);	  
	      
	      //derivates log(dp),log(dq)
	      for (i=0; i<N; i++)
		FLOATWRITE(file, model_derivative_p(i,dim));	  
	      
	      for (i=0; i<N; i++)
		FLOATWRITE(file, model_derivative_q(i,dim));    
	      
	      //derivates log(da),log(db)
	      for (i=0; i<N; i++)
		for (j=0; j<N; j++)
		  FLOATWRITE(file, model_derivative_a(i,j,dim));
	      
	      for (i=0; i<N; i++)
		for (j=0; j<M; j++)
		  FLOATWRITE(file, model_derivative_b(i,j,dim));
	      
	      if (dim==0)
		printf("Number of parameters (including posterior prob.): %i\n", num_floats) ;
	    } ;
	} 
      else
	{
	  if (file)
	    {
	      // write prob
	      FLOATWRITE(file, prob);	  
	      
	      for (i=0; model->get_learn_p(i)>=0; i++)
		FLOATWRITE(file, model_derivative_p(model->get_learn_p(i),dim));	  
	      
	      for (i=0; model->get_learn_q(i)>=0; i++)
		FLOATWRITE(file, model_derivative_q(model->get_learn_q(i),dim));    
	      
	      //derivates log(da),log(db)
	      for (q=0; model->get_learn_a(q,1)>=0; q++)
		{
		  i=model->get_learn_a(q,0) ;
		  j=model->get_learn_a(q,1) ;
		  FLOATWRITE(file, model_derivative_a(i,j,dim));
		} ;
	      
	      for (q=0; model->get_learn_b(q,0)>=0; q++)
		{
		  i=model->get_learn_b(q,0) ;
		  j=model->get_learn_b(q,1) ;
		  FLOATWRITE(file, model_derivative_b(i,j,dim));
		} ;
	      if (dim==0)
		printf("Number of parameters (including posterior prob.): %i\n", num_floats) ;
	    } ;
	} ;
    }
  save_model_bin(file) ;

#ifdef PARALLEL
  delete[] threads ;
  delete[] params ;
#endif
  
  result=true;
 CIO::message("\n") ;
  return result;
}

bool CHMM::save_model_derivatives(FILE* file)
{
  bool result=false;
  int dim,i,j;
  
  if (file)
    {
      
      fprintf(file,"%% lambda denotes the model\n%% O denotes the observation sequence\n%% Q denotes the path\n%%\n%% calculating derivatives of P[O|lambda]=sum_{all Q}p_{Q1}b_{Q1}(O_1}*a_{Q1}{Q2}b_{Q2}(O2)*...*q_{T-1}{T}b_{QT}(O_T}q_{Q_T} against p,q,a,b\n%%\n");
      fprintf(file,"%% dPr[...]=[ [dp_1,...,dp_N,dq_1,...,dq_N, da_11,da_12,..,da_1N,..,da_NN, db_11,.., db_NN]\n");
      fprintf(file,"%%            [dp_1,...,dp_N,dq_1,...,dq_N, da_11,da_12,..,da_1N,..,da_NN, db_11,.., db_NN]\n");
      fprintf(file,"%%                            .............................                                \n");
      fprintf(file,"%%            [dp_1,...,dp_N,dq_1,...,dq_N, da_11,da_12,..,da_1N,..,da_NN, db_11,.., db_MM]\n");
      fprintf(file,"%%          ];\n%%\n\nlog(dPr) = [\n");
      
      
      for (dim=0; dim<p_observations->get_DIMENSION(); dim++)
	{	
	  fprintf(file, "[ ");
	  
	  //derivates log(dp),log(dq)
	  for (i=0; i<N; i++)
	    fprintf(file,"%e, ", (double) model_derivative_p(i, dim) );		//log (dp)
	  
	  for (i=0; i<N; i++)
	    fprintf(file,"%e, ", (double) model_derivative_q(i, dim) );	//log (dq)
	  
	  //derivates log(da),log(db)
	  for (i=0; i<N; i++)
	    for (j=0; j<N; j++)
	      fprintf(file, "%e,", (double) model_derivative_a(i,j,dim) );
	  
	  for (i=0; i<N; i++)
	    for (j=0; j<M; j++)
	      fprintf(file, "%e,", (double) model_derivative_b(i,j,dim) );
	  
	  fseek(file,ftell(file)-1,SEEK_SET);
	  fprintf(file, " ];\n");
	}
      
      
      fprintf(file, "];");
      
      result=true;
    }
  return result;
}

#ifdef DEBUG
bool CHMM::check_model_derivatives()
{
	bool result=false;
	const REAL delta=3e-4 ;

	for (int dim=0; dim<p_observations->get_DIMENSION(); dim++)
	  {	
	    int i ;
	    //derivates log(dp),log(dq)
	    for (i=0; i<N; i++)
	      {
		for (int j=0; j<N; j++)
		  {
		    REAL old_a=get_a(i,j) ;
		
		    set_a(i,j, log(exp(old_a)-delta)) ;
		    invalidate_model() ;
		    REAL prob_old=exp(model_probability(dim)) ;
		    
		    set_a(i,j, log(exp(old_a)+delta)) ;
		    invalidate_model() ;
		    REAL prob_new=exp(model_probability(dim));
		    
		    REAL deriv = (prob_new-prob_old)/(2*delta) ;

		    set_a(i,j, old_a) ;
		    invalidate_model() ;
		    REAL deriv_calc=exp(model_derivative_a(i, j, dim)) ;
		    
		    CIO::message(stderr,"da(%i,%i) = %e:%e\t (%1.5f%%)\n", i,j, deriv_calc,  deriv, 100.0*(deriv-deriv_calc)/deriv_calc);		
		  } ;
	      } ;
	    for (i=0; i<N; i++)
	      {
		for (int j=0; j<M; j++)
		  {
		    REAL old_b=get_b(i,j) ;
		    
		    set_b(i,j, log(exp(old_b)-delta)) ;
		    invalidate_model() ;
		    REAL prob_old=exp(model_probability(dim)) ;

		    set_b(i,j, log(exp(old_b)+delta)) ;
		    invalidate_model() ;		    
		    REAL prob_new=exp(model_probability(dim));
		    
		    REAL deriv = (prob_new-prob_old)/(2*delta) ;

		    set_b(i,j, old_b) ;
		    invalidate_model() ;
		    REAL deriv_calc=exp(model_derivative_b(i, j, dim));
		    
		    CIO::message(stderr,"db(%i,%i) = %e:%e\t (%1.5f%%)\n", i,j, deriv_calc, deriv, 100.0*(deriv-deriv_calc)/(deriv_calc));		
		  } ;
	      } ;

	    for (i=0; i<N; i++)
	      {
		REAL old_p=get_p(i) ;
		
		set_p(i, log(exp(old_p)-delta)) ;
		invalidate_model() ;
		REAL prob_old=exp(model_probability(dim)) ;

		set_p(i, log(exp(old_p)+delta)) ;
		invalidate_model() ;		
		REAL prob_new=exp(model_probability(dim));
		REAL deriv = (prob_new-prob_old)/(2*delta) ;

		set_p(i, old_p) ;
		invalidate_model() ;
		REAL deriv_calc=exp(model_derivative_p(i, dim));
		
		//if (fabs(deriv_calc_old-deriv)>1e-4)
		  CIO::message(stderr,"dp(%i) = %e:%e\t (%1.5f%%)\n", i, deriv_calc, deriv, 100.0*(deriv-deriv_calc)/deriv_calc);		
	      } ;
	    for (i=0; i<N; i++)
	      {
		REAL old_q=get_q(i) ;
		
		set_q(i, log(exp(old_q)-delta)) ;
		invalidate_model() ;
		REAL prob_old=exp(model_probability(dim)) ;

		set_q(i, log(exp(old_q)+delta)) ;
		invalidate_model() ;		
		REAL prob_new=exp(model_probability(dim));

		REAL deriv = (prob_new-prob_old)/(2*delta) ;

		set_q(i, old_q) ;
		invalidate_model() ;		
		REAL deriv_calc=exp(model_derivative_q(i, dim)); 
		
		//if (fabs(deriv_calc_old-deriv)>1e-4)
		  CIO::message(stderr,"dq(%i) = %e:%e\t (%1.5f%%)\n", i, deriv_calc, deriv, 100.0*(deriv-deriv_calc)/deriv_calc);		
	      } ;
	  }
	return result;
}

bool CHMM::check_path_derivatives()
{
	bool result=false;
	const REAL delta=1e-4 ;

	for (int dim=0; dim<p_observations->get_DIMENSION(); dim++)
	  {	
	    int i ;
	    //derivates log(dp),log(dq)
	    for (i=0; i<N; i++)
	      {
		for (int j=0; j<N; j++)
		  {
		    REAL old_a=get_a(i,j) ;
		
		    set_a(i,j, log(exp(old_a)-delta)) ;
		    invalidate_model() ;
		    REAL prob_old=best_path(dim) ;
		    
		    set_a(i,j, log(exp(old_a)+delta)) ;
		    invalidate_model() ;
		    REAL prob_new=best_path(dim);
		    
		    REAL deriv = (prob_new-prob_old)/(2*delta) ;

		    set_a(i,j, old_a) ;
		    invalidate_model() ;
		    REAL deriv_calc=path_derivative_a(i, j, dim) ;
		    
		    CIO::message(stderr,"da(%i,%i) = %e:%e\t (%1.5f%%)\n", i,j, deriv_calc,  deriv, 100.0*(deriv-deriv_calc)/deriv_calc);		
		  } ;
	      } ;
	    for (i=0; i<N; i++)
	      {
		for (int j=0; j<M; j++)
		  {
		    REAL old_b=get_b(i,j) ;
		    
		    set_b(i,j, log(exp(old_b)-delta)) ;
		    invalidate_model() ;
		    REAL prob_old=best_path(dim) ;

		    set_b(i,j, log(exp(old_b)+delta)) ;
		    invalidate_model() ;		    
		    REAL prob_new=best_path(dim);
		    
		    REAL deriv = (prob_new-prob_old)/(2*delta) ;

		    set_b(i,j, old_b) ;
		    invalidate_model() ;
		    REAL deriv_calc=path_derivative_b(i, j, dim);
		    
		    CIO::message(stderr,"db(%i,%i) = %e:%e\t (%1.5f%%)\n", i,j, deriv_calc, deriv, 100.0*(deriv-deriv_calc)/(deriv_calc));		
		  } ;
	      } ;

	    for (i=0; i<N; i++)
	      {
		REAL old_p=get_p(i) ;
		
		set_p(i, log(exp(old_p)-delta)) ;
		invalidate_model() ;
		REAL prob_old=best_path(dim) ;

		set_p(i, log(exp(old_p)+delta)) ;
		invalidate_model() ;		
		REAL prob_new=best_path(dim);
		REAL deriv = (prob_new-prob_old)/(2*delta) ;

		set_p(i, old_p) ;
		invalidate_model() ;
		REAL deriv_calc=path_derivative_p(i, dim);
		
		//if (fabs(deriv_calc_old-deriv)>1e-4)
		  CIO::message(stderr,"dp(%i) = %e:%e\t (%1.5f%%)\n", i, deriv_calc, deriv, 100.0*(deriv-deriv_calc)/deriv_calc);		
	      } ;
	    for (i=0; i<N; i++)
	      {
		REAL old_q=get_q(i) ;
		
		set_q(i, log(exp(old_q)-delta)) ;
		invalidate_model() ;
		REAL prob_old=best_path(dim) ;

		set_q(i, log(exp(old_q)+delta)) ;
		invalidate_model() ;		
		REAL prob_new=best_path(dim);

		REAL deriv = (prob_new-prob_old)/(2*delta) ;

		set_q(i, old_q) ;
		invalidate_model() ;		
		REAL deriv_calc=path_derivative_q(i, dim); 
		
		//if (fabs(deriv_calc_old-deriv)>1e-4)
		  CIO::message(stderr,"dq(%i) = %e:%e\t (%1.5f%%)\n", i, deriv_calc, deriv, 100.0*(deriv-deriv_calc)/deriv_calc);		
		  } ;
	  }
	return result;
}
#endif // DEBUG 

//normalize model (sum to one constraint)
void CHMM::normalize()
{
    int i,j;
    const REAL INF=-1e10;
    REAL sum_p =INF;
    REAL sum_q =INF;

    for (i=0; i<N; i++)
    {
	sum_p=math.logarithmic_sum(sum_p, get_p(i));
	sum_q=math.logarithmic_sum(sum_q, get_q(i));
	
	REAL sum_a =INF;
	REAL sum_b =INF;

	for (j=0; j<N; j++)
	    sum_a=math.logarithmic_sum(sum_a, get_a(i,j));
	for (j=0; j<N; j++)
	    set_a(i,j, get_a(i,j)-sum_a);
	
	for (j=0; j<M; j++)
	    sum_b=math.logarithmic_sum(sum_b, get_b(i,j));
	for (j=0; j<M; j++)
	    set_b(i,j, get_b(i,j)-sum_b);
    }
    
    for (i=0; i<N; i++)
    {
	set_p(i, get_p(i)-sum_p);
	set_q(i, get_q(i)-sum_q);
    }
}
bool CHMM::append_model(CHMM* append_model, REAL* cur_out, REAL* app_out)
{
	bool result=false;
	const int num_states=append_model->get_N()+2;
	int i,j;

	if (append_model->get_M() == get_M())
	{
		REAL* n_p=new REAL[N+num_states];
		REAL* n_q=new REAL[N+num_states];
		REAL* n_a=new REAL[(N+num_states)*(N+num_states)];
		REAL* n_b=new REAL[(N+num_states)*M];

		//clear n_x 
		for (i=0; i<N+num_states; i++)
		{
			n_p[i]=-math.INFTY;
			n_q[i]=-math.INFTY;

			for (j=0; j<N+num_states; j++)
				n_a[(N+num_states)*j+i]=-math.INFTY;

			for (j=0; j<M; j++)
				n_b[M*i+j]=-math.INFTY;
		}

		//copy models first
		// warning pay attention to the ordering of 
		// transition_matrix_a, observation_matrix_b !!!

		// cur_model
		for (i=0; i<N; i++)
		{
			n_p[i]=get_p(i);

			for (j=0; j<N; j++)
				n_a[(N+num_states)*j+i]=get_a(i,j);

			for (j=0; j<M; j++)
			{
				n_b[M*i+j]=get_b(i,j);
				//CIO::message("bef: %f =", n_b[(N+num_states)*j+i]);
				//CIO::message("(i,j) = (%d,%d) := %f\n", i,j, get_b(i,j));
			}
		}

		// append_model
		for (i=0; i<append_model->get_N(); i++)
		{
			n_q[i+N+2]=append_model->get_q(i);

			for (j=0; j<append_model->get_N(); j++)
				n_a[(N+num_states)*(j+N+2)+(i+N+2)]=append_model->get_a(i,j);
			for (j=0; j<append_model->get_M(); j++)
				n_b[M*(i+N+2)+j]=append_model->get_b(i,j);
		}
		
		//initialize the two special states

		// output
		for (i=0; i<M; i++)
		{
			n_b[M*N+i]=cur_out[i];
			n_b[M*(N+1)+i]=app_out[i];
		}
	
		// transition to the two and back
		for (i=0; i<N+num_states; i++)
		{
			// the first state is only connected to the second
			if (i==N+1)
				n_a[(N+num_states)*i + N]=0;

			// only states of the cur_model can reach the
			// first state 
			if (i<N)
				n_a[(N+num_states)*N+i]=get_q(i);

			// the second state is only connected to states of
			// the append_model (with probab app->p(i))
			if (i>=N+2)
				n_a[(N+num_states)*i+(N+1)]=append_model->get_p(i-(N+2));
		}

		free_state_dependend_arrays();
		N+=num_states;
		
		alloc_state_dependend_arrays();

		//delete + adjust pointers
		delete[] initial_state_distribution_p;
		delete[] end_state_distribution_q;
		delete[] transition_matrix_a;
		delete[] observation_matrix_b;
		
		transition_matrix_a=n_a;
		observation_matrix_b=n_b;
		initial_state_distribution_p=n_p;
		end_state_distribution_q=n_q;

		invalidate_model();
		normalize();
	}

	return result;
}

void CHMM::add_states(int num_states, REAL default_value)
{
#define VAL_MACRO log((default_value == 0) ? ((MIN_RAND+((REAL)rand()))/(REAL(RAND_MAX/MAX_RAND))) : default_value)
	int i,j;
	const REAL MIN_RAND=1e-2; //this is the range of the random values for the new variables
	const REAL MAX_RAND=2e-1;

	REAL* n_p=new REAL[N+num_states];
	REAL* n_q=new REAL[N+num_states];
	REAL* n_a=new REAL[(N+num_states)*(N+num_states)];
	REAL* n_b=new REAL[(N+num_states)*M];

	// warning pay attention to the ordering of 
	// transition_matrix_a, observation_matrix_b !!!
	for (i=0; i<N; i++)
	{
		n_p[i]=get_p(i);
		n_q[i]=get_q(i);

		for (j=0; j<N; j++)
			n_a[(N+num_states)*j+i]=get_a(i,j);

		for (j=0; j<M; j++)
			n_b[M*i+j]=get_b(i,j);
	}

	for (i=N; i<N+num_states; i++)
	{
		n_p[i]=VAL_MACRO;
		n_q[i]=VAL_MACRO;

		for (j=0; j<N; j++)
			n_a[(N+num_states)*i+j]=VAL_MACRO;
	
		for (j=0; j<N+num_states; j++)
			n_a[(N+num_states)*j+i]=VAL_MACRO;

		for (j=0; j<M; j++)
			n_b[M*i+j]=VAL_MACRO;
	}
	free_state_dependend_arrays();
	N+=num_states;

	alloc_state_dependend_arrays();

	//delete + adjust pointers
	delete[] initial_state_distribution_p;
	delete[] end_state_distribution_q;
	delete[] transition_matrix_a;
	delete[] observation_matrix_b;

	transition_matrix_a=n_a;
	observation_matrix_b=n_b;
	initial_state_distribution_p=n_p;
	end_state_distribution_q=n_q;

	normalize();
	invalidate_model();
}

void CHMM::chop(REAL value)
{
    for (int i=0; i<N; i++)
    {
	int j;

	if (exp(get_p(i)) < value)
	    set_p(i, math.ALMOST_NEG_INFTY);

	if (exp(get_q(i)) < value)
	    set_q(i, math.ALMOST_NEG_INFTY);

	for (j=0; j<N; j++)
	{
	    if (exp(get_a(i,j)) < value)
		    set_a(i,j, math.ALMOST_NEG_INFTY);
	}

	for (j=0; j<M; j++)
	{
	    if (exp(get_b(i,j)) < value)
		    set_b(i,j, math.ALMOST_NEG_INFTY);
	}
    }
    normalize();
    invalidate_model();
}

bool CHMM::linear_train(FILE* file, const int WIDTH, const int UPTO)
{
    double* hist=new double[256*UPTO];
    char* line_=new char[WIDTH+1];

    int i;
    int total=0;

    for (i=0; i<UPTO; i++)
    {
	line_[i]=0;

	for (int j=0; j<256; j++)
	{
	    hist[i*256+j]=0;
	}
    }

    while ( (fread(line_, sizeof (unsigned char), WIDTH, file)) == (unsigned int) WIDTH)
    {
	for (i=0; i<UPTO; i++)
	{
	    hist[i*256+line_[i]]++;
	}		
	total++;
    }

    for (i=0;i<UPTO;i++)
    {
	for (int j=0; j<256; j++)
	    hist[i*256+j]=log(hist[i*256+j]+PSEUDO)-log(total+M*PSEUDO);
    }

    set_p(0, 0);
    for (i=1; i<UPTO; i++)
	set_p(i, math.ALMOST_NEG_INFTY);

    set_q(UPTO-1, 0);
    for (i=0; i<UPTO-1; i++)
	set_q(i, math.ALMOST_NEG_INFTY);


    for (i=0;i<UPTO;i++)
    {
	for (int j=0; j<UPTO; j++)
	{
	    if (i==j-1)
		set_a(i,j, 0);
	    else
		set_a(i,j, math.ALMOST_NEG_INFTY);
	}
    }

    for (i=0;i<UPTO;i++)
    {
	for (int j=0; j<M; j++)
	    set_b(i,j, hist[i*256+p_observations->remap(j)] );
    }
	delete[] line_;
	delete[] hist;
    return true;
}

REAL CHMM::linear_likelihood(FILE* file, int WIDTH, int UPTO, bool singleline)
{
    double* hist=new double[256*UPTO];
    char* line_=new char[WIDTH+1];

    int total=0;

    for (int i=0;i<N;i++)
    {
	for (int j=0; j<M; j++)
	    hist[i*256+p_observations->remap(j)]=get_b(i,j);
    }

    if (singleline)
    {
	double lik=-math.INFTY;
	if ( (fread(line_, sizeof (unsigned char), WIDTH, file)) == (unsigned int) WIDTH)
	{
	    double d=log(1);
	    for (int i=0; i<UPTO; i++)
		d+=hist[i*256+line_[i]];
	    
	    lik=d;
	}

	delete[] line_;
	delete[] hist;
	return lik;
    }
    else 
    {
	mod_prob=0.0;
	while ( (fread(line_, sizeof (unsigned char), WIDTH, file)) == (unsigned int) WIDTH)
	{
	    double d=log(1);
	    for (int i=0; i<UPTO; i++)
	    {
		d+=hist[i*256+line_[i]];
	    }

	    //CIO::message("P_AVG=%e\n",mod_prob);
	    mod_prob+=d;
	    total++;
	}
	//CIO::message("P_AVG=%e\n",mod_prob);
	delete[] line_;
	delete[] hist;

	mod_prob_updated=true;
	return mod_prob;
    }
}

bool CHMM::save_linear_likelihood_bin(FILE* src, FILE* dest, int WIDTH, int UPTO)
{
    double* hist=new double[256*UPTO];
    char* line_=new char[WIDTH+1];
    int total=0;

    mod_prob=0;

    for (int i=0;i<N;i++)
    {
	for (int j=0; j<M; j++)
	    hist[i*256+p_observations->remap(j)]=get_b(i,j);
    }

    while ( (fread(line_, sizeof (unsigned char), WIDTH, src)) == (unsigned int) WIDTH)
    {
	double d=log(1);
	for (int i=0; i<UPTO; i++)
	{
	    d+=hist[i*256+line_[i]];
	}
	float f=(float)d;
	fwrite(&f, sizeof(float),1, dest);
	mod_prob+=d;
	total++;
    }

    mod_prob_updated=true;
    
	delete[] line_;
	delete[] hist;

	return true ;
}

bool CHMM::save_linear_likelihood(FILE* src, FILE* dest, int WIDTH, int UPTO)
{
    double* hist=new double[256*UPTO];
    char* line_=new char[WIDTH+1];
    int total=0;

    mod_prob=0;

    for (int i=0;i<N;i++)
    {
	for (int j=0; j<M; j++)
	    hist[i*256+p_observations->remap(j)]=get_b(i,j);
    }

    fprintf(dest, "%% likelihood of model per observation\n%% P[O|model]=[ P[O|model]_1 P[O|model]_2 ... P[O|model]_dim ]\n%%\n");
    fprintf(dest, "P=[");

    while ( (fread(line_, sizeof (unsigned char), WIDTH, src)) == (unsigned int) WIDTH)
    {
	double d=log(1);
	for (int i=0; i<UPTO; i++)
	{
	    d+=hist[i*256+line_[i]];
	}

	fprintf(dest, "%e ", d);
	mod_prob+=d;
	total++;
    }

    fprintf(dest,"];\n\nP_AVG=%e\n",(double) mod_prob);

    mod_prob_updated=true;
	delete[] line_;
	delete[] hist;

	return true ;
}

void CHMM::set_observation_nocache(CObservation* obs)
{
    p_observations=obs;

	if (!reused_caches)
	{
#ifdef PARALLEL
		for (int i=0; i<NUM_PARALLEL; i++) 
		{
			delete[] alpha_cache[i].table;
			delete[] beta_cache[i].table;
			delete[] states_per_observation_psi[i];
			delete[] path[i];

			alpha_cache[i].table=NULL;
			beta_cache[i].table=NULL;
#ifndef NOVIT
			states_per_observation_psi[i]=NULL;
#endif // NOVIT
			path[i]=NULL;
		} ;
#else
		delete[] alpha_cache.table;
		delete[] beta_cache.table;
		delete[] states_per_observation_psi;
		delete[] path;

		alpha_cache.table=NULL;
		beta_cache.table=NULL;
		states_per_observation_psi=NULL;
		path=NULL;

#endif //PARALLEL
	}

	invalidate_model();
}

void CHMM::set_observations(CObservation* obs, CHMM* lambda)
{
    p_observations=obs;

	if (!reused_caches)
	{
#ifdef PARALLEL
		for (int i=0; i<NUM_PARALLEL; i++) 
		{
			delete[] alpha_cache[i].table;
			delete[] beta_cache[i].table;
#ifndef NOVIT
			delete[] states_per_observation_psi[i];
#endif // NOVIT
			delete[] path[i];

			alpha_cache[i].table=NULL;
			beta_cache[i].table=NULL;
#ifndef NOVIT
			states_per_observation_psi[i]=NULL;
#endif // NOVIT
			path[i]=NULL;
		} ;
#else
		delete[] alpha_cache.table;
		delete[] beta_cache.table;
		delete[] states_per_observation_psi;
		delete[] path;

		alpha_cache.table=NULL;
		beta_cache.table=NULL;
		states_per_observation_psi=NULL;
		path=NULL;

#endif //PARALLEL
	}

    if (obs!=NULL)
    {
	    int max_T=obs->get_obs_max_T();

		if (lambda)
		{
#ifdef PARALLEL
			for (int i=0; i<NUM_PARALLEL; i++) 
			{
				this->alpha_cache[i].table= lambda->alpha_cache[i].table;
				this->beta_cache[i].table=	lambda->beta_cache[i].table;
				this->states_per_observation_psi[i]=lambda->states_per_observation_psi[i] ;
				this->path[i]=lambda->path[i];
			} ;
#else
			this->alpha_cache.table= lambda->alpha_cache.table;
		    this->beta_cache.table= lambda->beta_cache.table;
		    this->states_per_observation_psi= lambda->states_per_observation_psi;
			this->path=lambda->path;
#endif //PARALLEL

			this->reused_caches=true;
		}
		else
		{
			this->reused_caches=false;
#ifdef PARALLEL
		   CIO::message("allocating mem for path-table of size %.2f Megabytes (%d*%d) each:\n", ((float)max_T)*N*sizeof(T_STATES)/(1024*1024), max_T, N);
			for (int i=0; i<NUM_PARALLEL; i++)
			{
			  if ((states_per_observation_psi[i]=new T_STATES[max_T*N])!=NULL)
					printf("path_table[%i] successfully allocated\n",i) ;
				else
					printf("failed allocating memory for path_table[%i].\n",i) ;
				path[i]=new T_STATES[max_T];
			}
#else // no PARALLEL 
			printf("allocating mem of size %.2f Megabytes (%d*%d) for path-table ....", ((float)max_T)*N*sizeof(T_STATES)/(1024*1024), max_T, N);
			if ((states_per_observation_psi=new T_STATES[max_T*N]) != NULL)
				printf("done.\n") ;
			else
				printf("failed.\n") ;
			
			path=new T_STATES[max_T];
#endif // PARALLEL
#ifndef NOCACHE
		   CIO::message("allocating mem for caches each of size %.2f Megabytes (%d*%d) ....\n", ((float)max_T)*N*sizeof(T_ALPHA_BETA_TABLE)/(1024*1024), max_T, N);

#ifdef PARALLEL
			for (int i=0; i<NUM_PARALLEL; i++)
			{
				if ((alpha_cache[i].table=new T_ALPHA_BETA_TABLE[max_T*N])!=NULL)
					printf("alpha_cache[%i].table successfully allocated\n",i) ;
				else
					printf("allocation of alpha_cache[%i].table failed\n",i) ;

				if ((beta_cache[i].table=new T_ALPHA_BETA_TABLE[max_T*N]) != NULL)
					printf("beta_cache[%i].table successfully allocated\n",i) ;
				else
					printf("allocation of beta_cache[%i].table failed\n",i) ;
			} ;
#else // PARALLEL
			if ((alpha_cache.table=new T_ALPHA_BETA_TABLE[max_T*N]) != NULL)
				printf("alpha_cache.table successfully allocated\n") ;
			else
				printf("allocation of alpha_cache.table failed\n") ;

			if ((beta_cache.table=new T_ALPHA_BETA_TABLE[max_T*N]) != NULL)
				printf("beta_cache.table successfully allocated\n") ;
			else
				printf("allocation of beta_cache.table failed\n") ;

#endif // PARALLEL
#else // NOCACHE
#ifdef PARALLEL
			for (int i=0; i<NUM_PARALLEL; i++)
			{
				alpha_cache[i].table=NULL ;
				beta_cache[i].table=NULL ;
			} ;
#else //PARALLEL
			alpha_cache.table=NULL ;
			beta_cache.table=NULL ;
#endif //PARALLEL
#endif //NOCACHE
	    }
	}

    //initialize pat/mod_prob as not calculated
    invalidate_model();
}

double* CHMM::compute_top_feature_vector(CHMM* pos, CHMM* neg, int dim, double* featurevector)
{

    if (!featurevector)
	{
		CIO::message("allocating %.2f M for top feature vector cache\n", 1+pos->get_N()*(1+pos->get_N()+1+pos->get_M()) + neg->get_N()*(1+neg->get_N()+1+neg->get_M()));
		featurevector=new double[ 1+pos->get_N()*(1+pos->get_N()+1+pos->get_M()) + neg->get_N()*(1+neg->get_N()+1+neg->get_M()) ];
	}

    if (!featurevector)
		return NULL;

    int i,j,p=0,x=dim;

    double posx=pos->model_probability(x);
    double negx=neg->model_probability(x);

    featurevector[p++]=(posx-negx);

#ifndef NORMALIZE_DERIVATIVE
    //first do positive model
    for (i=0; i<pos->get_N(); i++)
    {
	featurevector[p++]=exp(pos->model_derivative_p(i, x)-posx);
	featurevector[p++]=exp(pos->model_derivative_q(i, x)-posx);

	for (j=0; j<pos->get_N(); j++)
	    featurevector[p++]=exp(pos->model_derivative_a(i, j, x)-posx);

	for (j=0; j<pos->get_M(); j++)
	    featurevector[p++]=exp(pos->model_derivative_b(i, j, x)-posx);

    }
    
    for (i=0; i<neg->get_N(); i++)
    {
	featurevector[p++]= - exp(neg->model_derivative_p(i, x)-negx);
	featurevector[p++]= - exp(neg->model_derivative_q(i, x)-negx);

	for (j=0; j<neg->get_N(); j++)
	    featurevector[p++]= - exp(neg->model_derivative_a(i, j, x)-negx);

	for (j=0; j<neg->get_M(); j++)
	    featurevector[p++]= - exp(neg->model_derivative_b(i, j, x)-negx);
    }
#else
    int o_p=1;
    double sum_p=0;
    double sum_q=0;
    //first do positive model
    for (i=0; i<pos->get_N(); i++)
    {
	featurevector[p]=exp(pos->model_derivative_p(i, x)-posx);
	sum_p=exp(pos->get_p(i))*featurevector[p++];
	featurevector[p]=exp(pos->model_derivative_q(i, x)-posx);
	sum_q=exp(pos->get_q(i))*featurevector[p++];

	double sum_a=0;
	for (j=0; j<pos->get_N(); j++)
	{
	    featurevector[p]=exp(pos->model_derivative_a(i, j, x)-posx);
	    sum_a=exp(pos->get_a(i,j))*featurevector[p++];
	}
	p-=pos->get_N();
	for (j=0; j<pos->get_N(); j++)
	    featurevector[p++]-=sum_a;

	double sum_b=0;
	for (j=0; j<pos->get_M(); j++)
	{
	    featurevector[p]=exp(pos->model_derivative_b(i, j, x)-posx);
	    sum_b=exp(pos->get_b(i,j))*featurevector[p++];
	}
	p-=pos->get_M();
	for (j=0; j<pos->get_M(); j++)
	    featurevector[p++]-=sum_b;
    }

    o_p=p;
    p=1;
    for (i=0; i<pos->get_N(); i++)
    {
	featurevector[p++]-=sum_p;
	featurevector[p++]-=sum_q;
    }
    p=o_p;

    for (i=0; i<neg->get_N(); i++)
    {
	featurevector[p]=-exp(neg->model_derivative_p(i, x)-negx);
	sum_p=exp(neg->get_p(i))*featurevector[p++];
	featurevector[p]=-exp(neg->model_derivative_q(i, x)-negx);
	sum_q=exp(neg->get_q(i))*featurevector[p++];

	double sum_a=0;
	for (j=0; j<neg->get_N(); j++)
	{
	    featurevector[p]=-exp(neg->model_derivative_a(i, j, x)-negx);
	    sum_a=exp(neg->get_a(i,j))*featurevector[p++];
	}
	p-=neg->get_N();
	for (j=0; j<neg->get_N(); j++)
	    featurevector[p++]-=sum_a;

	double sum_b=0;
	for (j=0; j<neg->get_M(); j++)
	{
	    featurevector[p]=-exp(neg->model_derivative_b(i, j, x)-negx);
	    sum_b=exp(neg->get_b(i,j))*featurevector[p++];
	}
	p-=neg->get_M();
	for (j=0; j<neg->get_M(); j++)
	    featurevector[p++]-=sum_b;
    }

    p=o_p;
    for (i=0; i<neg->get_N(); i++)
    {
	featurevector[p++]-=sum_p;
	featurevector[p++]-=sum_q;
    }
#endif
    return featurevector;
}

void CHMM::check_and_update_crc(CHMM* pos, CHMM* neg)
{
    bool obs_result=false;
    bool sv_result=false;
    CObservation* o=pos->get_observations();

    if ( (feature_cache_checksums[0]!= (unsigned int) pos) ||
	 (feature_cache_checksums[1]!= (unsigned int) neg) ||
	 (feature_cache_checksums[2]!= (unsigned int) pos->get_N()) ||
	 (feature_cache_checksums[3]!= (unsigned int) pos->get_M()) ||
	 (feature_cache_checksums[4]!= (unsigned int) neg->get_N()) ||
	 (feature_cache_checksums[5]!= (unsigned int) neg->get_M())
       )
    {
	obs_result=true;
	sv_result=true;
    }

    if ((unsigned int) o->get_DIMENSION() != feature_cache_checksums[6])
	obs_result=true;
    
    const int CRCSIZE=32;
    const int CRCSIZEHALF=CRCSIZE/2;

    for (int i=0; i<CRCSIZEHALF && i<o->get_DIMENSION(); i++)
    {
	int idx=i*o->get_DIMENSION()/CRCSIZEHALF;
	unsigned int crc=math.crc32( (unsigned char*) o->get_obs_vector(idx), o->get_obs_T(idx) ); 

#ifdef DEBUG
	printf("OB:idx: %d maxl: %d crc: %x\n", idx, o->get_DIMENSION(), crc);
#endif
	if (features_crc32[i]!=crc)
	  {
	    features_crc32[i]=crc;
	    obs_result=true;
	  }
    }
    
    if (obs_result)
      {
	delete[] feature_cache_obs;
	feature_cache_obs=NULL;
      }
    
    if ((unsigned int) o->get_support_vector_num() != feature_cache_checksums[7])
      sv_result=true;
    
    {
      for (int i=0; i<CRCSIZEHALF && i<o->get_support_vector_num(); i++)
	{
	  int idx=o->get_support_vector_idx(i*o->get_support_vector_num()/CRCSIZEHALF);
	  unsigned int crc=math.crc32( (unsigned char*) o->get_obs_vector(idx), o->get_obs_T(idx) ); 
	  
#ifdef DEBUG
	 CIO::message("SV:idx: %d maxl: %d crc: %x\n", idx, o->get_support_vector_idx(0)+o->get_support_vector_num(), crc);
#endif
	  
	  if (features_crc32[i+CRCSIZEHALF]!=crc)
	    {
	      features_crc32[i+CRCSIZEHALF]=crc;
	      sv_result=true;
	    }
	}	
    } 

    if (sv_result)
    {
	delete[] feature_cache_sv;
	feature_cache_sv=NULL;
    }
    
    feature_cache_checksums[0]=(unsigned int) pos;
    feature_cache_checksums[1]=(unsigned int) neg;
    feature_cache_checksums[2]=(unsigned int) pos->get_N();
    feature_cache_checksums[3]=(unsigned int) pos->get_M();
    feature_cache_checksums[4]=(unsigned int) neg->get_N();
    feature_cache_checksums[5]=(unsigned int) neg->get_M();
    feature_cache_checksums[6]=(unsigned int) pos->get_observations()->get_DIMENSION();
    feature_cache_checksums[7]=(unsigned int) pos->get_observations()->get_support_vector_num();
    feature_cache_in_question=false;
}


void CHMM::invalidate_top_feature_cache(E_TOP_FEATURE_CACHE_VALIDITY v)
{
    switch (v)
    {
	case VALID:
	    break;
	case OBS_INVALID:
	    delete[] feature_cache_obs;
	    feature_cache_obs=NULL;
	    break;
	case SV_INVALID:
	    delete[] feature_cache_sv;
	    feature_cache_sv=NULL;
	    break;
	case QUESTIONABLE:
	    feature_cache_in_question=true;
	    break;
	case INVALID:
	    delete[] feature_cache_obs;
	    delete[] feature_cache_sv;
	    feature_cache_obs=NULL;
	    feature_cache_sv=NULL;
	    feature_cache_in_question=false;
	    num_features=0;
    };
}

#ifndef PARALLEL

bool CHMM::compute_top_feature_cache(CHMM* pos, CHMM* neg)
{
    num_features=1+ pos->get_N()*(1+pos->get_N()+1+pos->get_M()) + neg->get_N()*(1+neg->get_N()+1+neg->get_M());

    if (!feature_cache_sv || !feature_cache_obs || feature_cache_in_question )
    {
	check_and_update_crc(pos, neg);
	
	if (!feature_cache_sv)
	{
	   CIO::message("refreshing top_sv_feature_cache...........\n");

	    int totobs=pos->get_observations()->get_support_vector_num();
		CIO::message("allocating top feature cache of size %.2fM for sv\n", sizeof(double)*num_features*totobs/1024.0/1024.0);
	    feature_cache_sv=new double[num_features*totobs];

	   CIO::message("precalculating top feature vectors for support vectors\n");

		for (int x=0; x<totobs; x++)
		{
		    if (!(x % (totobs/10+1)))
			printf("%02d%%.", (int) (100.0*x/totobs));
		    else if (!(x % (totobs/200+1)))
			printf(".");

		   

		    compute_top_feature_vector(pos, neg, pos->get_observations()->get_support_vector_idx(x), &feature_cache_sv[x*num_features]);
		}
	    
		printf(".done.\n");
		
	}
	else
	   CIO::message("WARNING: using previous top_sv_feature_cache NOT recalculating\n");

	if (!feature_cache_obs)
	{
	   CIO::message("refreshing top_obs_feature_cache...........\n");

	    int totobs=pos->get_observations()->get_DIMENSION();
		CIO::message("allocating top feature cache of size %.2fM for obs\n", sizeof(double)*num_features*totobs/1024.0/1024.0);
	    feature_cache_obs=new double[num_features*totobs];

	   CIO::message("precalculating top feature vectors for observations\n");
	    
	    for (int x=0; x<totobs; x++)
	    {
		if (!(x % (totobs/10+1)))
		   CIO::message("%02d%%.", (int) (100.0*x/totobs));
		else if (!(x % (totobs/200+1)))
		   CIO::message(".");

		
		compute_top_feature_vector(pos, neg, x, &feature_cache_obs[x*num_features]);
	    }

	   CIO::message(".done.\n");
	   
	}
	else
	   CIO::message("WARNING: using previous top_obs_feature_cache NOT recalculating\n");
   
	if ((feature_cache_obs!=NULL) && (feature_cache_sv!=NULL || pos->get_observations()->get_support_vector_num() <= 0))
		return true;
	else 
	    return false;
    }
    else
    {
	   CIO::message("WARNING: using previous top_feature_cache NOT recalculating\n");
	    return true;
    }

}

#else

struct S_THREAD_PARAM2
{
  REAL* dest;
  CHMM * pos, *neg ;
  int    dim ;
}  ;

typedef struct S_THREAD_PARAM2 T_THREAD_PARAM2 ;

void *compute_top_feature_vector_helper(void * p)
{
  T_THREAD_PARAM2* params=((T_THREAD_PARAM2*)p) ;
  CHMM::compute_top_feature_vector(params->pos, params->neg, params->dim, params->dest) ;
  return NULL ;
} ;

bool CHMM::compute_top_feature_cache(CHMM* pos, CHMM* neg)
{
  pthread_t *threads=new pthread_t[NUM_PARALLEL] ;
  T_THREAD_PARAM2 *params=new T_THREAD_PARAM2[NUM_PARALLEL] ;

  num_features=1+ pos->get_N()*(1+pos->get_N()+1+pos->get_M()) + neg->get_N()*(1+neg->get_N()+1+neg->get_M());
    
  if (!feature_cache_sv || !feature_cache_obs || feature_cache_in_question )
    {
      check_and_update_crc(pos, neg);
      
      if (!feature_cache_sv)
	{
	 CIO::message("refreshing top_sv_feature_cache...........\n");
	  
	  int totobs=pos->get_observations()->get_support_vector_num();
	  feature_cache_sv=new double[num_features*totobs];
	  
	 CIO::message("precalculating top feature vectors for support vectors\n");
	  
	  for (int x=0; x<totobs; x++)
	    {
	      if (!(x % (totobs/10+1)))
		printf("%02d%%.", (int) (100.0*x/totobs));
	      else if (!(x % (totobs/200+1)))
		printf(".");
	      
	     

	      /* The following code calls the function
  		   compute_top_feature_vector(pos, neg, x, &feature_cache_sv[x*num_features]);
		 NUM_PARALLEL times (in parallel).
	      */
	      if (x%NUM_PARALLEL==0)
		{
		  int i ;
		  for (i=0; i<NUM_PARALLEL; i++)
		    if (x+i<pos->p_observations->get_DIMENSION())
		      {
			params[i].pos=pos ;
			params[i].neg=neg ;
			params[i].dim=x+i ;
			params[i].dest=&feature_cache_sv[x*num_features] ;
#ifdef SUNOS
			thr_create(NULL,0,compute_top_feature_vector_helper, (void*)&params[i], PTHREAD_SCOPE_SYSTEM, &threads[i]) ;
#else // SUNOS
			pthread_create(&threads[i], NULL, compute_top_feature_vector_helper, (void*)&params[i]) ;
#endif // SUNOS
		      } ;
		  for (i=0; i<NUM_PARALLEL; i++)
		    if (x+i<pos->p_observations->get_DIMENSION())
		      {
			void * ret ;
			pthread_join(threads[i], &ret) ;
		      } ;
		} ;
	    }
	  
	 CIO::message(".done.\n");
	 
	}
      else
	printf("WARNING: using previous top_sv_feature_cache NOT recalculating\n");
      
      if (!feature_cache_obs)
	{
	 CIO::message("refreshing top_obs_feature_cache...........\n");
	  
	  int totobs=pos->get_observations()->get_DIMENSION();
	  feature_cache_obs=new double[num_features*totobs];
	  
	 CIO::message("precalculating top feature vectors for observations\n");
	  
	  for (int x=0; x<totobs; x++)
	    {
	      if (!(x % (totobs/10+1)))
		printf("%02d%%.", (int) (100.0*x/totobs));
	      else if (!(x % (totobs/200+1)))
		printf(".");
	      
	     
	      
	      compute_top_feature_vector(pos, neg, x, &feature_cache_obs[x*num_features]);
	    }
	  
	 CIO::message(".done.\n");
	 
	}
      else
	printf("WARNING: using previous top_obs_feature_cache NOT recalculating\n");
      
      if ((feature_cache_obs!=NULL) && (feature_cache_sv!=NULL || pos->get_observations()->get_support_vector_num() <= 0))
	return true;
      else 
	return false;
    }
  else
    {
     CIO::message("WARNING: using previous top_feature_cache NOT recalculating\n");
      return true;
    }
  
}
#endif
