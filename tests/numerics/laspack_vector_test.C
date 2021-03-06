#include <libmesh/laspack_vector.h>

#ifdef HAVE_LASPACK

#include "numeric_vector_test.h"

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCase.h>

using namespace libMesh;

class LaspackVectorTest : public NumericVectorTest<LaspackVector<Real> > { 
public: 
  CPPUNIT_TEST_SUITE( LaspackVectorTest );

  if (libMesh::n_processors() == 1)
  {
    NUMERICVECTORTEST
  }
  
  CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION( LaspackVectorTest );

#endif // #ifdef HAVE_LASPACK

