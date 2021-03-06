// The libMesh Finite Element Library.
// Copyright (C) 2002-2012 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA



// C++ includes

// Local Includes
#include "libmesh/dof_map.h"
#include "libmesh/laspack_matrix.h"
#include "libmesh/eigen_sparse_matrix.h"
#include "libmesh/parallel.h"
#include "libmesh/petsc_matrix.h"
#include "libmesh/sparse_matrix.h"
#include "libmesh/trilinos_epetra_matrix.h"
#include "libmesh/numeric_vector.h"

namespace libMesh
{


//------------------------------------------------------------------
// SparseMatrix Methods


// Constructor
template <typename T>
SparseMatrix<T>::SparseMatrix (const Parallel::Communicator &comm) :
  ParallelObject(comm),
  _dof_map(NULL),
  _is_initialized(false)
{}



// Destructor
template <typename T>
SparseMatrix<T>::~SparseMatrix ()
{}



// Full specialization of print method for Complex datatypes
template <>
void SparseMatrix<Complex>::print(std::ostream& os, const bool sparse) const
{
  // std::complex<>::operator<<() is defined, but use this form

  if(sparse)
    {
      libmesh_not_implemented();
    }

  os << "Real part:" << std::endl;
  for (numeric_index_type i=0; i<this->m(); i++)
    {
      for (numeric_index_type j=0; j<this->n(); j++)
	os << std::setw(8) << (*this)(i,j).real() << " ";
      os << std::endl;
    }

  os << std::endl << "Imaginary part:" << std::endl;
  for (numeric_index_type i=0; i<this->m(); i++)
    {
      for (numeric_index_type j=0; j<this->n(); j++)
	os << std::setw(8) << (*this)(i,j).imag() << " ";
      os << std::endl;
    }
}






// Full specialization for Real datatypes
template <typename T>
AutoPtr<SparseMatrix<T> >
SparseMatrix<T>::build(const Parallel::Communicator &comm,
		       const SolverPackage solver_package)
{
  // Build the appropriate vector
  switch (solver_package)
    {


#ifdef LIBMESH_HAVE_LASPACK
    case LASPACK_SOLVERS:
      {
	AutoPtr<SparseMatrix<T> > ap(new LaspackMatrix<T>(comm));
	return ap;
      }
#endif


#ifdef LIBMESH_HAVE_PETSC
    case PETSC_SOLVERS:
      {
	AutoPtr<SparseMatrix<T> > ap(new PetscMatrix<T>(comm));
	return ap;
      }
#endif


#ifdef LIBMESH_HAVE_TRILINOS
    case TRILINOS_SOLVERS:
      {
	AutoPtr<SparseMatrix<T> > ap(new EpetraMatrix<T>(comm));
	return ap;
      }
#endif


#ifdef LIBMESH_HAVE_EIGEN
    case EIGEN_SOLVERS:
      {
	AutoPtr<SparseMatrix<T> > ap(new EigenSparseMatrix<T>(comm));
	return ap;
      }
#endif


    default:
      libMesh::err << "ERROR:  Unrecognized solver package: "
		    << solver_package
		    << std::endl;
      libmesh_error();
    }

  AutoPtr<SparseMatrix<T> > ap(NULL);
  return ap;
}


template <typename T>
void SparseMatrix<T>::vector_mult (NumericVector<T>& dest,
				   const NumericVector<T>& arg) const
{
  dest.zero();
  this->vector_mult_add(dest,arg);
}



template <typename T>
void SparseMatrix<T>::vector_mult_add (NumericVector<T>& dest,
				       const NumericVector<T>& arg) const
{
  /* This functionality is actually implemented in the \p
     NumericVector class.  */
  dest.add_vector(arg,*this);
}



template <typename T>
void SparseMatrix<T>::zero_rows (std::vector<numeric_index_type> &, T)
{
  /* This functionality isn't implemented or stubbed in every subclass yet */
  libmesh_not_implemented();
}



template <typename T>
void SparseMatrix<T>::print(std::ostream& os, const bool sparse) const
{
  parallel_object_only();

  libmesh_assert (this->initialized());

  if(!this->_dof_map)
  {
    os << std::endl << "Error!  Trying to print a matrix with no dof_map set!" << std::endl << std::endl;
    libmesh_error();
  }

  // We'll print the matrix from processor 0 to make sure
  // it's serialized properly
  if (this->processor_id() == 0)
    {
      libmesh_assert_equal_to (this->_dof_map->first_dof(), 0);
      for (numeric_index_type i=this->_dof_map->first_dof();
           i!=this->_dof_map->end_dof(); ++i)
        {
	  if(sparse)
	    {
	      for (numeric_index_type j=0; j<this->n(); j++)
		{
		  T c = (*this)(i,j);
		  if (c != static_cast<T>(0.0))
		    {
		      os << i << " " << j << " " << c << std::endl;
		    }
		}
	    }
	  else
	    {
	      for (numeric_index_type j=0; j<this->n(); j++)
		os << (*this)(i,j) << " ";
	      os << std::endl;
	    }
        }

      std::vector<numeric_index_type> ibuf, jbuf;
      std::vector<T> cbuf;
      numeric_index_type currenti = this->_dof_map->end_dof();
      for (processor_id_type p=1; p < this->n_processors(); ++p)
        {
          this->comm().receive(p, ibuf);
          this->comm().receive(p, jbuf);
          this->comm().receive(p, cbuf);
          libmesh_assert_equal_to (ibuf.size(), jbuf.size());
          libmesh_assert_equal_to (ibuf.size(), cbuf.size());

          if (ibuf.empty())
            continue;
          libmesh_assert_greater_equal (ibuf.front(), currenti);
          libmesh_assert_greater_equal (ibuf.back(), ibuf.front());

	  std::size_t currentb = 0;
          for (;currenti <= ibuf.back(); ++currenti)
            {
	      if(sparse)
		{
		  for (numeric_index_type j=0; j<this->n(); j++)
		    {
		      if (currentb < ibuf.size() &&
			  ibuf[currentb] == currenti &&
			  jbuf[currentb] == j)
			{
			  os << currenti << " " << j << " " << cbuf[currentb] << std::endl;
			  currentb++;
			}
		    }
		}
	      else
		{
		  for (numeric_index_type j=0; j<this->n(); j++)
		    {
		      if (currentb < ibuf.size() &&
			  ibuf[currentb] == currenti &&
			  jbuf[currentb] == j)
			{
			  os << cbuf[currentb] << " ";
			  currentb++;
			}
		      else
			os << static_cast<T>(0.0) << " ";
		    }
		  os << std::endl;
		}
            }
        }
      if(!sparse)
	{
	  for (; currenti != this->m(); ++currenti)
	    {
	      for (numeric_index_type j=0; j<this->n(); j++)
		os << static_cast<T>(0.0) << " ";
	      os << std::endl;
	    }
	}
    }
  else
    {
      std::vector<numeric_index_type> ibuf, jbuf;
      std::vector<T> cbuf;

      // We'll assume each processor has access to entire
      // matrix rows, so (*this)(i,j) is valid if i is a local index.
      for (numeric_index_type i=this->_dof_map->first_dof();
           i!=this->_dof_map->end_dof(); ++i)
        {
          for (numeric_index_type j=0; j<this->n(); j++)
	    {
              T c = (*this)(i,j);
              if (c != static_cast<T>(0.0))
                {
                  ibuf.push_back(i);
                  jbuf.push_back(j);
                  cbuf.push_back(c);
                }
	    }
        }
      this->comm().send(0,ibuf);
      this->comm().send(0,jbuf);
      this->comm().send(0,cbuf);
    }
}



//------------------------------------------------------------------
// Explicit instantiations
template class SparseMatrix<Number>;

} // namespace libMesh
