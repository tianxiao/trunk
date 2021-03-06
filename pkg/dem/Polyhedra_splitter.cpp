// © 2013 Jan Elias, http://www.fce.vutbr.cz/STM/elias.j/, elias.j@fce.vutbr.cz
// https://www.vutbr.cz/www_base/gigadisk.php?i=95194aa9a

#ifdef YADE_CGAL

#include<yade/pkg/dem/Polyhedra_splitter.hpp>


YADE_PLUGIN((PolyhedraSplitter));
CREATE_LOGGER(PolyhedraSplitter);

//*********************************************************************************
/* Evaluate tensorial stress estimation in polyhedras */

void getStressForEachBody(vector<Matrix3r>& bStresses){
	const shared_ptr<Scene>& scene=Omega::instance().getScene();
	bStresses.resize(scene->bodies->size());
	for (size_t k=0;k<scene->bodies->size();k++) bStresses[k]=Matrix3r::Zero();
	FOREACH(const shared_ptr<Interaction>& I, *scene->interactions){
		if(!I->isReal()) continue;
		PolyhedraGeom* geom=YADE_CAST<PolyhedraGeom*>(I->geom.get());
		PolyhedraPhys* phys=YADE_CAST<PolyhedraPhys*>(I->phys.get());
		if(!geom || !phys) continue;
		Vector3r f=phys->normalForce+phys->shearForce;
		//Sum f_i*l_j for each contact of each particle
		bStresses[I->getId1()] -=f*((geom->contactPoint-Body::byId(I->getId1(),scene)->state->pos).transpose());
		bStresses[I->getId2()] +=f*((geom->contactPoint-Body::byId(I->getId2(),scene)->state->pos).transpose());

		
	}
}

//*********************************************************************************
/* Size dependent strength */

double PolyhedraSplitter::getStrength(double volume, double strength){
	//equvalent radius
	double r_eq = pow(volume*3./4./Mathr::PI,1./3.);
	//r should be in milimeters
	return strength/(r_eq/1000.);
}

//*********************************************************************************
/* Symmetrization of stress tensor */

void PolyhedraSplitter::Symmetrize(Matrix3r & bStress){
	bStress(0,1) = (bStress(0,1) + bStress(1,0))/2.;
	bStress(0,2) = (bStress(0,2) + bStress(2,0))/2.;
	bStress(1,2) = (bStress(1,2) + bStress(2,1))/2.;
	bStress(1,0) = bStress(0,1);
	bStress(2,0) = bStress(0,2);
	bStress(2,1) = bStress(1,2);
}

//*********************************************************************************
/* Split if stress exceed strength */

void PolyhedraSplitter::action()
{
	const shared_ptr<Scene> _rb=shared_ptr<Scene>();
	shared_ptr<Scene> rb=(_rb?_rb:Omega::instance().getScene());

	vector<shared_ptr<Body> > bodies;
	vector<Vector3r > directions;
	vector<double > sigmas;



	vector<Matrix3r> bStresses;
	getStressForEachBody(bStresses);

	int i = -1;
	FOREACH(const shared_ptr<Body>& b, *rb->bodies){
		i++;
		if(!b || !b->material || !b->shape) continue;
		shared_ptr<Polyhedra> p=dynamic_pointer_cast<Polyhedra>(b->shape);
		shared_ptr<PolyhedraMat> m=dynamic_pointer_cast<PolyhedraMat>(b->material);
	
		if(p && m->IsSplitable){
			//not real strees, to get real one, it has to be divided by body volume
			Matrix3r stress = bStresses[i];

			//get eigenstresses
			Symmetrize(stress);
			Matrix3r I_vect(Matrix3r::Zero()), I_valu(Matrix3r::Zero()); 
			matrixEigenDecomposition(stress,I_vect,I_valu);	
			int min_i = 0;
			if (I_valu(min_i,min_i) > I_valu(1,1)) min_i = 1;
			if (I_valu(min_i,min_i) > I_valu(2,2)) min_i = 2;	
			int max_i = 0;
			if (I_valu(max_i,max_i) < I_valu(1,1)) max_i = 1;
			if (I_valu(max_i,max_i) < I_valu(2,2)) max_i = 2;
			
			//division of stress by volume
			double comp_stress = I_valu(min_i,min_i)/p->GetVolume();
			double tens_stress = I_valu(max_i,max_i)/p->GetVolume();
			Vector3r dirC = I_vect.col(max_i);
			Vector3r dirT = I_vect.col(min_i);
			Vector3r dir  = dirC.normalized() + dirT.normalized();;	
			double sigma_t = -comp_stress/2.+ tens_stress;
			if (sigma_t > getStrength(p->GetVolume(),m->GetStrength())) {bodies.push_back(b); directions.push_back(dir); sigmas.push_back(sigma_t);};
		}		
	}
	for(int i=0; i<int(bodies.size()); i++){
		SplitPolyhedra(bodies[i], directions[i]);
	}
}

#endif // YADE_CGAL
