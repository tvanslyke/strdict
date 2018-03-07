#ifndef PY_SUPPORT_H
#define PY_SUPPORT_H
#include <algorithm>
#include <utility>
#include <iterator>

template <class It, class LeftPred, class RightPred>
std::pair<It, It> trim_range(It first, It last, LeftPred lpred, RightPred rpred)
{
	first = std::find_if_not(first, last, lpred);
	last = std::find_if_not(
		std::make_reverse_iterator(last), 
		std::make_reverse_iterator(first), 
		rpred
	).base();
	return std::make_pair(first, last);
}

template <class It, class Pred>
std::pair<It, It> trim_range(It first, It last, Pred pred)
{
	return trim_range(first, last, lpred, pred, pred);
}


template <class Scalar>


std::string_view unicode_to_string_view(PyObject* arg)
{
	
}

template <class ... Args>
auto arg_parse_tuple(PyObject* tuple)
{
	
}


#endif /* PY_SUPPORT_H */
