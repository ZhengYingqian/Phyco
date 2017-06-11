/* Copyright 2017 by Yifei Zheng
 * This file is part of ATOM.
 * Unauthorized copy, modification or distribution is prohibited.
 *
 * This file defines functions that are used to convert data from and to Python.
 */

#include "../common/python_common.hpp"
#include "rule_types.hpp"
#include "cNVar.hpp"
#include <vector>

PyObject* tuplize_order(const ResolvingOrder& order) {
    try {
        auto ret = PyExc(PyList_New(0), nullptr);
        for (auto& step : order.seq) {
            if (step->type() == 0) {
                std::size_t rule_id = static_cast<ResolvingOrder::step<EqnSolver::ALG_S>*>(step.get())->rule_id;
                const NVar& solve_for = static_cast<ResolvingOrder::step<EqnSolver::ALG_S>*>(step.get())->solve_for;
                auto tuple = PyTuple_New(2);
                PyOnly(PyTuple_SetItem(tuple, 0, PyLong_FromLong(rule_id)), 0);
                PyOnly(PyTuple_SetItem(tuple, 1, make_NVar(solve_for)), 0);
                PyList_Append(ret, tuple);
            }
            else if (step->type() == 1) {
                const auto& rule_id = static_cast<ResolvingOrder::step<EqnSolver::ALG_M>*>(step.get())->rules;
                const auto& solve_for = static_cast<ResolvingOrder::step<EqnSolver::ALG_M>*>(step.get())->solve_for;
                auto tuple = PyTuple_New(2);
                auto rules = PyList_New(0);
                auto solves = PyList_New(0);
                for (std::size_t i : rule_id)
                    PyList_Append(rules, PyLong_FromLong(i));
                for (const NVar& i : solve_for)
                    PyList_Append(rules, make_NVar(i));
                PyOnly(PyTuple_SetItem(tuple, 0, rules), 0);
                PyOnly(PyTuple_SetItem(tuple, 1, solves), 0);
                PyList_Append(ret, tuple);
            }
            else {
                const NVar& from = static_cast<ResolvingOrder::step<EqnSolver::DIFF_S>*>(step.get())->from;
                const NVar& to = static_cast<ResolvingOrder::step<EqnSolver::DIFF_S>*>(step.get())->to;
                auto tuple = PyTuple_New(2);
                PyOnly(PyTuple_SetItem(tuple, 0, make_NVar(from)), 0);
                PyOnly(PyTuple_SetItem(tuple, 1, make_NVar(to)), 0);
                PyList_Append(ret, tuple);
            }
        }
        return ret;
    }
    catch (Python_API_Exception) {
        PyErr_SetString(PyExc_RuntimeError, "Unknown error occurred in tuplize_order which shouldn't throw");
        return nullptr;
    }
}

PyObject* resolve(PyObject*, PyObject* args) {
    std::vector<Rule> Pack;
    CSF_flat_set<NVar, NVar::Less> requests;
    std::unordered_map<std::string, CSF_flat_set<std::size_t>> all_forms;
    try {
        PyObject* pack;
        PyObject* solve_for;
        PyOnly(PyArg_ParseTuple(args, "OO", &pack, &solve_for), true);
        std::size_t sz = PyExc(PySequence_Size(pack), -1);
        Pack.reserve(sz);
        for (std::size_t i = 0; i != sz; ++i) {
            auto r = PyList_GET_ITEM(pack, i);
            PyScoped diffs(PyExc(PyObject_GetAttrString(r, "diffs"), nullptr));
            PyScoped iter(PyExc(PyObject_GetIter(diffs), nullptr));
            PyScoped item(PyIter_Next(iter));
            CSF_set<NVar> vars;
            for (; item; item = PyScoped(PyIter_Next(iter))) {
                const cNVar* i = static_cast<cNVar*>(item.get());
                vars.insert(i->var);
                all_forms[i->var.name].insert(i->var.order);
            }
            auto idx = PyExc(PyObject_GetAttrString(r, "idx"), nullptr);
            Pack.emplace_back(PyLong_AsLong(idx), std::move(vars));
        }
        PyScoped iter(PyExc(PyObject_GetIter(solve_for), nullptr));
        PyScoped item(PyIter_Next(iter));
        for (; item; item = PyScoped(PyIter_Next(iter))) {
            const cNVar* i = static_cast<cNVar*>(item.get());
            requests.insert(i->var);
        }
    }
    catch (Python_API_Exception) {
        PyErr_SetString(PyExc_RuntimeError, "Error converting python objects to C++ equivalent");
        return nullptr;
    }
    ResolvingOrder order;
    RuleResolver Resolver(std::move(Pack), order, requests, std::move(all_forms));
    try {
        Resolver.process({});
    }
    catch (RulePackCannotBeResolved) {
        PyErr_SetString(PyExc_ValueError, "Rule pack cannot be resolved");
        return nullptr;
    }
    return tuplize_order(order);
}
