#ifndef PYBINDINSTANCE_HPP_
# define PYBINDINSTANCE_HPP_

#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/eval.h>
#include <map>
#include <functional>

namespace py = pybind11;

class Controllable;
struct PyEvaluate;

#define PYTHONMODULE "resources/scripts/pythonModule.py"

class	PyBindInstance
{
public:
    PyBindInstance();
    ~PyBindInstance() = default;

public:
    // Import a .py file.
    py::object import(const std::string &mod,
        const std::string &path, py::object &glb);
    void chasePlayerAI(Controllable &, PyEvaluate &);
    void fleePlayerAI(Controllable &, PyEvaluate &);
    void chaseEnemyAI(Controllable &, PyEvaluate &);
    void fleeEnemyAI(Controllable &, PyEvaluate &);
    std::map<unsigned int, std::function<void(PyBindInstance *, Controllable &, PyEvaluate &)>> execAI;

private:
    py::object main;
    py::object globals;
    py::object pythonModule;
};

#endif // !PYBINDINSTANCE_HPP_
