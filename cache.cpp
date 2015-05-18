#include "cache.h" 

/* Fill this in with parameters that will give you the
   best CPI and AMAT. */
void tune_parameters(Core *c) {
        int L1_size = 16*1024;
	int V_size = 16*1024/2;
	int L2_size = 300*1024 - L1_size - V_size;
	c->setL1Attributes( L1_size, 1, 512);
	c->setVictimAttributes( V_size, 1, 512);
//	c->setL2Attributes(205792, 8, 512);
	c->setL2Attributes(L2_size, 16, 512);
	c->setEvictionPolicy(0);
}

/* Implement this function. 
 * Returns true if there is a cache hit.
 * Arguments: ldst - 0 if load, 1 if store.
 *            address - Memory address of the reference.
 *            cycle - Number of cycles executed so far.
 */

int getTag( int address, int sz, int asc, int blk )
{

    int idx = sz/(blk*asc); 
    int idx_bit = ceil( log(idx)/log(2));
    int blk_offset = ceil(log(blk)/log(2));
    int tag_size = 32 - blk_offset - idx_bit ;

    return (address >> (blk_offset + idx_bit)) & ( (1 << tag_size) - 1); 

}

int getIdx ( int address, int sz, int asc, int blk )
{
    int idx = sz/(blk*asc); 
    int idx_bit = ceil( log(idx)/log(2) );
    int blk_offset = ceil( log(blk)/log(2) );
    return  ( (address >> blk_offset ) & ( (1 << idx_bit) - 1)) %idx; 
}

bool Cache::isHit(int ldst, int address, int cycle) {
    int idx_address = getIdx(address, getSize(),     getAssociativity(), getBlockSize() ); 
    int tag_address = getTag(address, getSize(),    getAssociativity(), getBlockSize() ) ;
    for ( int i = 0 ; i < getAssociativity() ; i++)
    {
	if( getTag( (tag[idx_address ][ i ]), getSize(),    getAssociativity(), getBlockSize())  
		    == tag_address){ 
	    accessed[idx_address ] [i] = cycle;
	    return true;
	}
    }
    return false;
}


/* Implement this function.
 * Returns the number of cycles taken to perform the memory reference.
 * Arguments: ldst - 0 if load, 1 if store.
 *            address - Memory address of the reference.
 *            cycle - Number of cycles executed so far.
 * Function calls you might make: isHit, getters in Cache.
 * Data structures you might need to read and/or update: 
 *           tags and accessed arrays in Cache.
 */
int Core::accessCache(int ldst, int address, int cycles) {

    l1Accesses++;
    totalMemRef++;
    // L1 hit, return L1 hit latency
    if(L1.isHit( ldst, address , cycles) == true){
	int l2_idx = getIdx( address, L2.getSize(), L2.getAssociativity(), L2.getBlockSize());
	int hit_tag_in_l2 = getTag(address , L2.getSize(), L2.getAssociativity(), L2.getBlockSize()); 

	bool L2HasL1 = false;
	for( int i = 0; i< L2.getAssociativity(); i++){
	    if( hit_tag_in_l2 ==   getTag( L2.tag[l2_idx][i],L2.getSize(), L2.getAssociativity(), L2.getBlockSize()) )
	    {
		L2HasL1 = true;
		L2.accessed[l2_idx][i] = cycles;
		break;
	    }
	}
	return L1.getHitLatency();
    }
    else{
    // not hit L1
    // check victime size
	l1Misses++;
	int vic_array_size =  Victim.getSize()/ Victim.getBlockSize()/ Victim.getAssociativity() ;
	//if(Victim.getSize() > 0){
	if(vic_array_size > 0){
	    // not hit L1 
	    victimAccesses++;
	    if(Victim.isHit( ldst , address, cycles ) == true){

		// Victim hit
		// find the l1 oldest based on LRU

		int l2_idx = getIdx( address, L2.getSize(), L2.getAssociativity(), L2.getBlockSize());
		int l2_tag = getTag( address, L2.getSize(), L2.getAssociativity(), L2.getBlockSize());
		int l2_tmp_flag = 0;
		int l2_tmp_accessed = L2.accessed[l2_idx][0];
		bool L2Has= false;
		for( int i = 0 ; i < L2.getAssociativity(); i++){
		    if( l2_tag == getTag( L2.tag[l2_idx][i], L2.getSize(), L2.getAssociativity(), L2.getBlockSize()) ){ 
			// has the value
			L2Has = true;
			l2_tmp_flag = i;
			break;
		    }
		    // does not have this value, evict l2_tmp_flag
		    if( l2_tmp_accessed > L2.accessed[l2_idx][i]){
			l2_tmp_flag = i;
		    } 
		}

		L2.tag[l2_idx][l2_tmp_flag] = address;
		L2.accessed[l2_idx][l2_tmp_flag] = cycles;

		int l1_idx = getIdx( address, L1.getSize(), L1.getAssociativity(), L1.getBlockSize());
		int l1_tag = getTag( address, L1.getSize(), L1.getAssociativity(), L1.getBlockSize());
		int l2_evict_tag_l1 = getTag( L2.tag[l2_idx][l2_tmp_flag], L1.getSize(), L1.getAssociativity(), L1.getBlockSize());
		int l1_tmp_flag = 0 ;
		int l1_tmp_accessed = L1.accessed[l1_idx][0] ;
		for( int i = 0; i< L1.getAssociativity(); i++){
		    if( l2_evict_tag_l1 == getTag( L1.tag[l1_idx][i], L1.getSize(), L1.getAssociativity(), L1.getBlockSize()) ){
			l1_tmp_flag = i;
			break;
		    }
		    if(l1_tmp_accessed > L1.accessed[l1_idx][i]){
			l1_tmp_flag = i;
		    }
		}
		//////////// 
		// replace L1 to victim hit position
		int vic_idx = getIdx( address,  Victim.getSize(), Victim.getAssociativity(), Victim.getBlockSize());
		int vic_tag = getTag( address, Victim.getSize(), Victim.getAssociativity(), Victim.getBlockSize());
		for( int i = 0 ; i < Victim.getAssociativity(); i++){
		    //if( address == Victim.tag[vic_idx][i]) {
		    if( vic_tag  ==
			getTag(Victim.tag[vic_idx][i],Victim.getSize(), Victim.getAssociativity(), Victim.getBlockSize())) 
		    {

			Victim.tag[vic_idx][i] = L1.tag[l1_idx][l1_tmp_flag] ;
			Victim.accessed[vic_idx][i] = L1.accessed[l1_idx][l1_tmp_flag] ;
		    } 
		}

		// update the value in LRU of L1
		L1.tag[l1_idx][l1_tmp_flag] = address;
		L1.accessed[l1_idx][l1_tmp_flag] = cycles;



		return Victim.getHitLatency();
		}
		else{
		    // miss victim hit then go to L2
		    victimMisses++;
		    l2Accesses++;
		    if( L2.isHit(ldst, address, cycles ) == true){
			// L2 is hit
			// find l1's LRU
			int l1_idx = getIdx( address, L1.getSize(), L1.getAssociativity(), L1.getBlockSize());
			int l1_tmp_flag = 0 ;
			int l1_tmp_accessed = L1.accessed[l1_idx][0] ;

			for( int i = 0; i< L1.getAssociativity(); i++){
			    if(l1_tmp_accessed > L1.accessed[l1_idx][i]){
				l1_tmp_flag = i;
				l1_tmp_accessed = L1.accessed[l1_idx][i];
			    }
			}

			// find victim's LRU 
			int vic_idx = getIdx( address, Victim.getSize(), Victim.getAssociativity(), Victim.getBlockSize());
			int vic_tmp_flag = 0;
			int vic_tmp_accessed = Victim.accessed[vic_idx][0];
			for( int i = 0 ; i < Victim.getAssociativity(); i++){
			    if( vic_tmp_accessed > Victim.accessed[vic_idx][i]) {
				vic_tmp_flag = i;
				vic_tmp_accessed = Victim.accessed[vic_idx][i];
			    } 
			}

			// receive L1's eviction
			Victim.tag[vic_idx][vic_tmp_flag] = L1.tag[l1_idx][l1_tmp_flag];
			Victim.accessed[vic_idx][vic_tmp_flag] = L1.accessed[l1_idx][l1_tmp_flag];

			// update the value from L2 
			L1.tag[l1_idx][l1_tmp_flag] = address;
			L1.accessed[l1_idx][l1_tmp_flag] = cycles;

			// update L2, where L2.isHit(...) is doing this job.

			//return //(min( L1.getHitLatency() ,Victim.getHitLatency()) + L2.getHitLatency() + L1L2Transfer);
			return L2.getHitLatency() + L1L2Transfer;
		    }
		    else{
			// L2 miss
			l2Misses++;
		    }
		}
	    }
	    else{

//	    victimAccesses=1;
//	    victimMisses=0;
	    // not hit L1 and no victim 
	    l2Accesses++;
	    if( L2.isHit(ldst, address, cycles ) == true){

		int l1_idx = getIdx( address, L1.getSize(), L1.getAssociativity(), L1.getBlockSize());

		int l1_tmp_flag = 0 ;
		int l1_tmp_accessed = L1.accessed[l1_idx][0] ;
		for( int i = 1; i< L1.getAssociativity(); i++){
		    if(l1_tmp_accessed > L1.accessed[l1_idx][i]){
			l1_tmp_flag = i;
			l1_tmp_accessed = L1.accessed[l1_idx][i];
		    }
		}
		L1.tag[l1_idx][l1_tmp_flag] = address;
		L1.accessed[l1_idx][l1_tmp_flag] = cycles;

		return L2.getHitLatency() + L1L2Transfer;
	    }
	    else{
		l2Misses++;
	    }
	}
    }
    int l2_idx = getIdx( address, L2.getSize(), L2.getAssociativity(), L2.getBlockSize());
    int l2_tag = getTag( address, L2.getSize(), L2.getAssociativity(), L2.getBlockSize());

    int l2_tmp_flag = 0 ;
    int l2_tmp_accessed = L2.accessed[l2_idx][0] ;

    // find L2's LRU
    for( int i = 0; i< L2.getAssociativity(); i++){
	if(l2_tmp_accessed > L2.accessed[l2_idx][i]){
	    l2_tmp_flag = i;
	    l2_tmp_accessed = L2.accessed[l2_idx][i];
	}
    }


    // L1, L2 and victim miss
    int l1_idx = getIdx( address, L1.getSize(), L1.getAssociativity(), L1.getBlockSize());
//    int l1_tag = getTag( address, L1.getSize(), L1.getAssociativity(), L1.getBlockSize());

    int l1_tmp_flag = 0 ;
    int l1_tmp_accessed = L1.accessed[l1_idx][0] ;
    bool L1hasL2 = false;
    int l2_tag_in_l1 = getTag(  L2.tag[l2_idx][l2_tmp_flag], L1.getSize(), L1.getAssociativity(), L1.getBlockSize());
    // find L1's LRU
    for( int i = 0; i< L1.getAssociativity(); i++){
	if( getTag( L1.tag[l1_idx][i],L1.getSize(), L1.getAssociativity(), L1.getBlockSize())
		    == l2_tag_in_l1) {
	    L1hasL2 = true;
	    l1_tmp_flag = i;
	    break;
	    // once found the address, break!
	}
	// else choose LRU
	if(l1_tmp_accessed > L1.accessed[l1_idx][i]){
	    l1_tmp_flag = i;
	    l1_tmp_accessed = L1.accessed[l1_idx][i];
	}
    }


    // find victime if has to recieve the eviction from L1 

    int vic_array_size =  Victim.getSize()/ Victim.getBlockSize()/ Victim.getAssociativity() ;
   // if(Victim.getSize()>0){
    if( vic_array_size > 0 ){
	int vic_idx = getIdx( address,  Victim.getSize(), Victim.getAssociativity(), Victim.getBlockSize()  );
	int vic_tag = getTag( address, Victim.getSize(), Victim.getAssociativity(), Victim.getBlockSize());
	int vic_tmp_flag = 0;
	int vic_tmp_accessed = Victim.accessed[vic_idx][0];
	for( int i = 0 ; i < Victim.getAssociativity(); i++){
	    if( vic_tmp_accessed > Victim.accessed[vic_idx][i]){
		vic_tmp_flag = i;
		vic_tmp_accessed = Victim.accessed[vic_idx][i];
	    }
	}
	Victim.tag[vic_idx][vic_tmp_flag] = L1.tag[l1_idx][l1_tmp_flag] ;
	Victim.accessed[vic_idx][ vic_tmp_flag ] = L1.accessed[l1_idx][l1_tmp_flag] ;
    }

    L1.tag[l1_idx][l1_tmp_flag] = address;
    L1.accessed[l1_idx][l1_tmp_flag] = cycles;
    L2.tag[l2_idx][l2_tmp_flag] = address;
    L2.accessed[l2_idx][l2_tmp_flag] = cycles;
    return memoryAccessTime;

}

/* Implement the following functions. */
double Core::getL1MissRate() {
    return static_cast<double>(l1Misses) / l1Accesses;
}

double Core::getVictimLocalMissRate() {
//    cout<<" victim "<< victimMisses<<" ; "<< victimAccesses<<endl;
    if(victimAccesses == 0 ) return 0;
    return static_cast<double>(victimMisses) / victimAccesses;
}

double Core::getL2LocalMissRate() {
   return static_cast<double>(l2Misses ) / l2Accesses;
}

double Core::getL2GlobalMissRate() {


    return static_cast<double>(l2Misses )/ (totalMemRef);

}

