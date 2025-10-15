#ifndef SCOPEGUARD_H_
#define SCOPEGUARD_H_

/*
 * Scopeguard, by Andrei Alexandrescu and Petru Marginean, December 2000.
 * Modified by Joshua Lehrer, FactSet Research Systems, November 2005.
 * Based on code from the Loki Library (Andrei Alexandrescu)
 * Licensed under the MIT License: https://github.com/snaewe/loki-lib/blob/7c29d87ecdecd60e39db969125753f6a4f549131/include/loki/ScopeGuard.h#L2-L24
 *
 * Modified for CCP, December 2007
 *    Added Comments, recommended usage, and support for multiple call types on member functions (__stdcall)
 *
 * Please see: http://www.zete.org/people/jlehrer/scopeguard.html
 *
 * ScopeGuard exists to make your life and handling of exceptions easier.
 * There are many cases where you want to make sure that you turn the lights off before
 * you leave the room, or return something to a safe state before you leave the scope of
 * a function, and this is what ScopeGuard exists to do.
 *
 * ScopeGuard currently goes up to functions (or member functions) with 3 parameters.
 * If more are needed, they can be added by following the same pattern very very carefully.
 *
 * CCP Recommended Usage:
 *
 * Do use ScopeGuard to return things to their correct values to guard against exceptions
 * and early return statements (this makes usage of CR_RETURN and CR_RETURN_VAL much easier)
 *
 * Do not use ScopeGuard as a member object, or use pointers to scopeguard objects etc.
 *
 * For normal functions, use:
 * 1) ScopeGuard guard = MakeGuard( &Function );
 * or
 * 2) ON_BLOCK_EXIT( &Function );
 *
 * depending on if you want an anonymous scope guard (2) or one that you can dismiss (1)
 *
 * Similarly, for member functions, use:
 * 1) ScopeGuard guard = MakeGuard( &objectClass::Function, object );
 * or
 * 2) ON_BLOCK_EXIT( &objectClass::Function, object );
 *
 * (objectClass is actually the class upon which the function is defined, not nessecarily the same as the type of the object )
 *
 * Example:
 *
 *  // In this case, we flip the render target to a temporary one within a function, but must
 *  // make sure that things are changed back when we leave.
 *
 *	CR_RETURN_VAL( mD3DDevice->GetRenderTarget( 0, &oldTarget ), 0);    		
 *	CR_RETURN_VAL( mD3DDevice->SetRenderTarget(0, target) , 0 );
 *	ON_BLOCK_EXIT( &IDirect3DDevice9::SetRenderTarget, mD3DDevice.p, 0, oldTarget );
 *
 * Example 2:
 *
 * // In this case, we set up a ScopeGuard to undo the changes we made, only if we exit prematurely during the
 * // CR_RETURN, otherwise, the changes we made are kept. In this case, we do not use the macros because we need a named variable to dismiss.
 *
 * DoSomething();
 * ScopeGuard guard = MakeGuard(&UndoSomething);
 * CR_RETURN( ... );
 * guard.Dismiss();
 *
 * NB:
 *
 * Watch out for invoking a ScopeGuard object within an "if" statement, since the body of it is a block
 * You could end up with unexpected behaviour if the function is invoked before you expect it
 */

// -------------------------------------------------------------------------------
// RefHolder
// -------------------------------------------------------------------------------
//
// A duplication of functionality implemented in boost, this template allows you
// to copy a reference for use in the functors below by wrapping it with a RefHolder
// that is copied instead.
//
// Invoke using ByRef( myRef )
// -------------------------------------------------------------------------------
template <class RefType>
class RefHolder
{
	RefType& ref_;
public:
	RefHolder(RefType& ref) : ref_(ref) {}
	operator RefType& () const 
	{
		return ref_;
	}
private:
    // Disable assignment - not implemented
    RefHolder& operator=(const RefHolder&);
};

// Creation function for RefHolder, to avoid making the user define the class template type
template <class RefType>
inline RefHolder<RefType> ByRef(RefType& ref)
{
	return RefHolder<RefType>(ref);
}

// -------------------------------------------------------------------------------
// ScopeGuardImplBase
// -------------------------------------------------------------------------------
//
// ScopeGuardImplBase is the base class of the various scope guards that are created
// according to the templated parameter requirements of the function you are trying to
// wrap in a scope guard object.

class ScopeGuardImplBase
{
	ScopeGuardImplBase& operator =(const ScopeGuardImplBase&);
protected:
	~ScopeGuardImplBase()
	{
	}
	ScopeGuardImplBase(const ScopeGuardImplBase& other) throw() 
		: dismissed_(other.dismissed_)
	{
		other.Dismiss();
	}
	template <typename ScopeObjectType>
	static void SafeExecute(ScopeObjectType& so) throw() 
	{
		if (!so.dismissed_)
			try
			{
				so.Execute();
			}
			catch(...)
			{
			}
	}
	
	mutable bool dismissed_;
public:
	ScopeGuardImplBase() throw() : dismissed_(false) 
	{
	}
	void Dismiss() const throw() 
	{
		dismissed_ = true;
	}
};

typedef const ScopeGuardImplBase& ScopeGuard;
// -------------------------------------------------------------------------------

// ScopeGuard for a 0 parameter function
template <typename FunctionType>
class ScopeGuardImpl0 : public ScopeGuardImplBase
{
public:
	static ScopeGuardImpl0<FunctionType> MakeGuard(FunctionType fun)
	{
		return ScopeGuardImpl0<FunctionType>(fun);
	}
	~ScopeGuardImpl0() throw() 
	{
		SafeExecute(*this);
	}
	void Execute() 
	{
		fun_();
	}
protected:
	ScopeGuardImpl0(FunctionType fun) : fun_(fun) 
	{
	}
	FunctionType fun_;
};

// MakeGuard for a 0 parameter function guard object
template <typename FunctionType> 
inline ScopeGuardImpl0<FunctionType> MakeGuard(FunctionType fun)
{
	return ScopeGuardImpl0<FunctionType>::MakeGuard(fun);
}

// ScopeGuard for a 1 parameter function
template <typename FunctionType, typename P1Type>
class ScopeGuardImpl1 : public ScopeGuardImplBase
{
public:
	static ScopeGuardImpl1<FunctionType, P1Type> MakeGuard(FunctionType fun, P1Type p1)
	{
		return ScopeGuardImpl1<FunctionType, P1Type>(fun, p1);
	}
	~ScopeGuardImpl1() throw() 
	{
		SafeExecute(*this);
	}
	void Execute()
	{
		fun_(p1_);
	}
protected:
	ScopeGuardImpl1(FunctionType fun, P1Type p1) : fun_(fun), p1_(p1) 
	{
	}
	FunctionType fun_;
	const P1Type p1_;
};

// MakeGuard for a 1 parameter function guard object
template <typename FunctionType, typename P1Type> 
inline ScopeGuardImpl1<FunctionType, P1Type> MakeGuard(FunctionType fun, P1Type p1)
{
	return ScopeGuardImpl1<FunctionType, P1Type>::MakeGuard(fun, p1);
}

// ScopeGuard for a 2 parameter function
template <typename FunctionType, typename P1Type, typename P2Type>
class ScopeGuardImpl2: public ScopeGuardImplBase
{
public:
	static ScopeGuardImpl2<FunctionType, P1Type, P2Type> MakeGuard(FunctionType fun, P1Type p1, P2Type p2)
	{
		return ScopeGuardImpl2<FunctionType, P1Type, P2Type>(fun, p1, p2);
	}
	~ScopeGuardImpl2() throw() 
	{
		SafeExecute(*this);
	}
	void Execute()
	{
		fun_(p1_, p2_);
	}
protected:
	ScopeGuardImpl2(FunctionType fun, P1Type p1, P2Type p2) : fun_(fun), p1_(p1), p2_(p2) 
	{
	}
	FunctionType fun_;
	const P1Type p1_;
	const P2Type p2_;
};

// MakeGuard for a 2 parameter function guard object
template <typename FunctionType, typename P1Type, typename P2Type>
inline ScopeGuardImpl2<FunctionType, P1Type, P2Type> MakeGuard(FunctionType fun, P1Type p1, P2Type p2)
{
	return ScopeGuardImpl2<FunctionType, P1Type, P2Type>::MakeGuard(fun, p1, p2);
}

// ScopeGuard for a 3 parameter function
template <typename FunctionType, typename P1Type, typename P2Type, typename P3Type>
class ScopeGuardImpl3 : public ScopeGuardImplBase
{
public:
	static ScopeGuardImpl3<FunctionType, P1Type, P2Type, P3Type> MakeGuard(FunctionType fun, P1Type p1, P2Type p2, P3Type p3)
	{
		return ScopeGuardImpl3<FunctionType, P1Type, P2Type, P3Type>(fun, p1, p2, p3);
	}
	~ScopeGuardImpl3() throw() 
	{
		SafeExecute(*this);
	}
	void Execute()
	{
		fun_(p1_, p2_, p3_);
	}
protected:
	ScopeGuardImpl3(FunctionType fun, P1Type p1, P2Type p2, P3Type p3) : fun_(fun), p1_(p1), p2_(p2), p3_(p3) 
	{
	}
	FunctionType fun_;
	const P1Type p1_;
	const P2Type p2_;
	const P3Type p3_;
};

// MakeGuard for a 3 parameter function guard object
template <typename FunctionType, typename P1Type, typename P2Type, typename P3Type>
inline ScopeGuardImpl3<FunctionType, P1Type, P2Type, P3Type> MakeGuard(FunctionType fun, P1Type p1, P2Type p2, P3Type p3)
{
	return ScopeGuardImpl3<FunctionType, P1Type, P2Type, P3Type>::MakeGuard(fun, p1, p2, p3);
}

//************************************************************
// Member function scope guards.
//
// This is where things get really complicated. In order to call a member function, you need
// the object to call the function on, and the member function pointer.
// Member functions differ in type from standard functions, and you also run into the issue that
// member functions can be defined on base classes.
//
// The MakeGuard functions abstract this issue away from the user through template type resolution
// magic.

template <class ObjectType, typename MemberFunctionType>
class ObjScopeGuardImpl0 : public ScopeGuardImplBase
{
public:
	static ObjScopeGuardImpl0<ObjectType, MemberFunctionType> MakeObjGuard(ObjectType& obj, MemberFunctionType memberFunction)
	{
		return ObjScopeGuardImpl0<ObjectType, MemberFunctionType>(obj, memberFunction);
	}
	~ObjScopeGuardImpl0() throw() 
	{
		SafeExecute(*this);
	}
	void Execute() 
	{
		(obj_.*memFun_)();
	}
protected:
	ObjScopeGuardImpl0(ObjectType& obj, MemberFunctionType memberFunction) 
		: obj_(obj), memFun_(memberFunction) {}
	ObjectType& obj_;
	MemberFunctionType memFun_;
};

template <class ObjectType, typename MemberFunctionType>
inline ObjScopeGuardImpl0<ObjectType, MemberFunctionType> MakeObjGuard(ObjectType& obj, MemberFunctionType memberFunction)
{
	return ObjScopeGuardImpl0<ObjectType, MemberFunctionType>::MakeObjGuard(obj, memberFunction);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType>
inline ObjScopeGuardImpl0<ObjectType,ReturnType(FunctionBaseType::*)()> MakeGuard(ReturnType(FunctionBaseType::*memberFunction)(), ObjectType &obj) {
  return ObjScopeGuardImpl0<ObjectType,ReturnType(FunctionBaseType::*)()>::MakeObjGuard(obj, memberFunction);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType>
inline ObjScopeGuardImpl0<ObjectType,ReturnType(FunctionBaseType::*)()> MakeGuard(ReturnType(FunctionBaseType::*memberFunction)(), ObjectType *obj) {
  return ObjScopeGuardImpl0<ObjectType,ReturnType(FunctionBaseType::*)()>::MakeObjGuard(*obj, memberFunction);
}

#if _WIN32 && !defined(_WIN64)

// 64 bit compiler doesn't care about vs __stdcall so only distinguish between
// them in 32 bit compilation
template <typename ReturnType, class ObjectType, class FunctionBaseType>
inline ObjScopeGuardImpl0<ObjectType,ReturnType(__stdcall FunctionBaseType::*)()> MakeGuard(ReturnType(__stdcall FunctionBaseType::*memberFunction)(), ObjectType &obj) {
  return ObjScopeGuardImpl0<ObjectType,ReturnType(__stdcall FunctionBaseType::*)()>::MakeObjGuard(obj, memberFunction);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType>
inline ObjScopeGuardImpl0<ObjectType,ReturnType(__stdcall FunctionBaseType::*)()> MakeGuard(ReturnType(__stdcall FunctionBaseType::*memberFunction)(), ObjectType *obj) {
  return ObjScopeGuardImpl0<ObjectType,ReturnType(__stdcall FunctionBaseType::*)()>::MakeObjGuard(*obj, memberFunction);
}

#endif

template <class ObjectType, typename MemberFunctionType, typename P1Type>
class ObjScopeGuardImpl1 : public ScopeGuardImplBase
{
public:
	static ObjScopeGuardImpl1<ObjectType, MemberFunctionType, P1Type> MakeObjGuard(ObjectType& obj, MemberFunctionType memberFunction, P1Type p1)
	{
		return ObjScopeGuardImpl1<ObjectType, MemberFunctionType, P1Type>(obj, memberFunction, p1);
	}
	~ObjScopeGuardImpl1() throw() 
	{
		SafeExecute(*this);
	}
	void Execute() 
	{
		(obj_.*memFun_)(p1_);
	}
protected:
	ObjScopeGuardImpl1(ObjectType& obj, MemberFunctionType memberFunction, P1Type p1) 
		: obj_(obj), memFun_(memberFunction), p1_(p1) {}
	ObjectType& obj_;
	MemberFunctionType memFun_;
	const P1Type p1_;
};

template <class ObjectType, typename MemberFunctionType, typename P1Type>
inline ObjScopeGuardImpl1<ObjectType, MemberFunctionType, P1Type> MakeObjGuard(ObjectType& obj, MemberFunctionType memberFunction, P1Type p1)
{
	return ObjScopeGuardImpl1<ObjectType, MemberFunctionType, P1Type>::MakeObjGuard(obj, memberFunction, p1);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType, typename P1TypeA, typename P1TypeB>
inline ObjScopeGuardImpl1<ObjectType,ReturnType(FunctionBaseType::*)(P1TypeA),P1TypeB> MakeGuard(ReturnType(FunctionBaseType::*memberFunction)(P1TypeA), ObjectType &obj, P1TypeB p1) {
  return ObjScopeGuardImpl1<ObjectType,ReturnType(FunctionBaseType::*)(P1TypeA),P1TypeB>::MakeObjGuard(obj, memberFunction, p1);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType, typename P1TypeA, typename P1TypeB>
inline ObjScopeGuardImpl1<ObjectType,ReturnType(FunctionBaseType::*)(P1TypeA),P1TypeB> MakeGuard(ReturnType(FunctionBaseType::*memberFunction)(P1TypeA), ObjectType *obj, P1TypeB p1) {
  return ObjScopeGuardImpl1<ObjectType,ReturnType(FunctionBaseType::*)(P1TypeA),P1TypeB>::MakeObjGuard(*obj, memberFunction, p1);
}

#if _WIN32 && !defined(_WIN64)

// 64 bit compiler doesn't care about vs __stdcall so only distinguish between
// them in 32 bit compilation

template <typename ReturnType, class ObjectType, class FunctionBaseType, typename P1TypeA, typename P1TypeB>
inline ObjScopeGuardImpl1<ObjectType,ReturnType(__stdcall FunctionBaseType::*)(P1TypeA),P1TypeB> MakeGuard(ReturnType(__stdcall FunctionBaseType::*memberFunction)(P1TypeA), ObjectType &obj, P1TypeB p1) {
  return ObjScopeGuardImpl1<ObjectType,ReturnType(__stdcall FunctionBaseType::*)(P1TypeA),P1TypeB>::MakeObjGuard(obj, memberFunction, p1);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType, typename P1TypeA, typename P1TypeB>
inline ObjScopeGuardImpl1<ObjectType,ReturnType(__stdcall FunctionBaseType::*)(P1TypeA),P1TypeB> MakeGuard(ReturnType(__stdcall FunctionBaseType::*memberFunction)(P1TypeA), ObjectType *obj, P1TypeB p1) {
  return ObjScopeGuardImpl1<ObjectType,ReturnType(__stdcall FunctionBaseType::*)(P1TypeA),P1TypeB>::MakeObjGuard(*obj, memberFunction, p1);
}

#endif

template <class ObjectType, typename MemberFunctionType, typename P1Type, typename P2Type>
class ObjScopeGuardImpl2 : public ScopeGuardImplBase
{
public:
	static ObjScopeGuardImpl2<ObjectType, MemberFunctionType, P1Type, P2Type> MakeObjGuard(ObjectType& obj, MemberFunctionType memberFunction, P1Type p1, P2Type p2)
	{
		return ObjScopeGuardImpl2<ObjectType, MemberFunctionType, P1Type, P2Type>(obj, memberFunction, p1, p2);
	}
	~ObjScopeGuardImpl2() throw() 
	{
		SafeExecute(*this);
	}
	void Execute() 
	{
		(obj_.*memFun_)(p1_, p2_);
	}
protected:
	ObjScopeGuardImpl2(ObjectType& obj, MemberFunctionType memberFunction, P1Type p1, P2Type p2) 
		: obj_(obj), memFun_(memberFunction), p1_(p1), p2_(p2) {}
	ObjectType& obj_;
	MemberFunctionType memFun_;
	const P1Type p1_;
	const P2Type p2_;
};

template <class ObjectType, typename MemberFunctionType, typename P1Type, typename P2Type>
inline ObjScopeGuardImpl2<ObjectType, MemberFunctionType, P1Type, P2Type> MakeObjGuard(ObjectType& obj, MemberFunctionType memberFunction, P1Type p1, P2Type p2)
{
	return ObjScopeGuardImpl2<ObjectType, MemberFunctionType, P1Type, P2Type>::MakeObjGuard(obj, memberFunction, p1, p2);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType, typename P1TypeA, typename P1TypeB, typename P2TypeA, typename P2TypeB>
inline ObjScopeGuardImpl2<ObjectType,ReturnType(FunctionBaseType::*)(P1TypeA,P2TypeA),P1TypeB,P2TypeB> MakeGuard(ReturnType(FunctionBaseType::*memberFunction)(P1TypeA,P2TypeA), ObjectType &obj, P1TypeB p1, P2TypeB p2) {
  return ObjScopeGuardImpl2<ObjectType,ReturnType(FunctionBaseType::*)(P1TypeA,P2TypeA),P1TypeB,P2TypeB>::MakeObjGuard(obj,memberFunction,p1,p2);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType, typename P1TypeA, typename P1TypeB, typename P2TypeA, typename P2TypeB>
inline ObjScopeGuardImpl2<ObjectType,ReturnType(FunctionBaseType::*)(P1TypeA,P2TypeA),P1TypeB,P2TypeB> MakeGuard(ReturnType(FunctionBaseType::*memberFunction)(P1TypeA,P2TypeA), ObjectType *obj, P1TypeB p1, P2TypeB p2) {
  return ObjScopeGuardImpl2<ObjectType,ReturnType(FunctionBaseType::*)(P1TypeA,P2TypeA),P1TypeB,P2TypeB>::MakeObjGuard(*obj,memberFunction,p1,p2);
}

#if _WIN32 && !defined(_WIN64)

// 64 bit compiler doesn't care about vs __stdcall so only distinguish between
// them in 32 bit compilation

template <typename ReturnType, class ObjectType, class FunctionBaseType, typename P1TypeA, typename P1TypeB, typename P2TypeA, typename P2TypeB>
inline ObjScopeGuardImpl2<ObjectType,ReturnType(__stdcall FunctionBaseType::*)(P1TypeA,P2TypeA),P1TypeB,P2TypeB> MakeGuard(ReturnType(__stdcall FunctionBaseType::*memberFunction)(P1TypeA,P2TypeA), ObjectType &obj, P1TypeB p1, P2TypeB p2) {
  return ObjScopeGuardImpl2<ObjectType,ReturnType(__stdcall FunctionBaseType::*)(P1TypeA,P2TypeA),P1TypeB,P2TypeB>::MakeObjGuard(obj,memberFunction,p1,p2);
}

template <typename ReturnType, class ObjectType, class FunctionBaseType, typename P1TypeA, typename P1TypeB, typename P2TypeA, typename P2TypeB>
inline ObjScopeGuardImpl2<ObjectType,ReturnType(__stdcall FunctionBaseType::*)(P1TypeA,P2TypeA),P1TypeB,P2TypeB> MakeGuard(ReturnType(__stdcall FunctionBaseType::*memberFunction)(P1TypeA,P2TypeA), ObjectType *obj, P1TypeB p1, P2TypeB p2) {
  return ObjScopeGuardImpl2<ObjectType,ReturnType(__stdcall FunctionBaseType::*)(P1TypeA,P2TypeA),P1TypeB,P2TypeB>::MakeObjGuard(*obj,memberFunction,p1,p2);
}

#endif

// Macros. CONCATENATE is some crazy compiler magic to stringify __LINE__
#define CONCATENATE_DIRECT(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_DIRECT(s1, s2)
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __LINE__)

#define ON_BLOCK_EXIT ScopeGuard ANONYMOUS_VARIABLE(scopeGuard) = MakeGuard
#define ON_BLOCK_EXIT_WITH_UNUSED(v) ScopeGuard ANONYMOUS_VARIABLE(scopeGuard) = MakeGuard(v); ANONYMOUS_VARIABLE(scopeGuard)

// Deprecated, please use ON_BLOCK_EXIT, with the member function first
#define ON_BLOCK_EXIT_OBJ ScopeGuard ANONYMOUS_VARIABLE(scopeGuard) = MakeObjGuard

#endif

