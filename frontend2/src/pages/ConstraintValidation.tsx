import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { Shield, CheckCircle, AlertTriangle } from 'lucide-react';

export default function ConstraintValidation() {
  const { currentResult } = useApp();

  if (!currentResult) {
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center">
        <div className="text-center">
          <p className="text-gray mb-4">No optimization results available</p>
          <Link to="/upload" className="btn-primary">Start New Optimization</Link>
        </div>
      </div>
    );
  }

  const { hard, soft } = currentResult.constraints;
  const totalConstraints = hard.total + soft.total;
  const totalSatisfied = hard.satisfied + soft.satisfied;

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-6xl mx-auto">
        <div className="mb-8">
          <h1 className="text-4xl font-bold mb-2">Constraint Compliance Report</h1>
          <div className="flex items-center gap-3">
            <span className="badge badge-success text-base px-4 py-2">
              {totalSatisfied} of {totalConstraints} constraints fully satisfied
            </span>
          </div>
        </div>

        {/* Compliance Overview */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="card mb-6">
          <h2 className="text-2xl font-bold mb-6">Compliance Overview</h2>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
            <div>
              <div className="flex items-center gap-3 mb-4">
                <Shield className="w-8 h-8 text-red-400" />
                <h3 className="text-xl font-bold">Hard Constraints</h3>
              </div>
              <div className="space-y-3">
                <div className="flex justify-between">
                  <span className="text-gray">Total:</span>
                  <span className="font-bold">{hard.total}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray">Satisfied:</span>
                  <span className="font-bold text-green-400">{hard.satisfied}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray">Violated:</span>
                  <span className="font-bold text-red-400">{hard.violated}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray">Compliance Rate:</span>
                  <span className="font-bold text-primary">{hard.complianceRate}%</span>
                </div>
              </div>
              <div className="mt-4 h-2 bg-dark-600 rounded-full overflow-hidden">
                <div className="h-full bg-green-500" style={{ width: `${hard.complianceRate}%` }} />
              </div>
            </div>

            <div>
              <div className="flex items-center gap-3 mb-4">
                <Shield className="w-8 h-8 text-yellow-400" />
                <h3 className="text-xl font-bold">Soft Constraints</h3>
              </div>
              <div className="space-y-3">
                <div className="flex justify-between">
                  <span className="text-gray">Total:</span>
                  <span className="font-bold">{soft.total}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray">Satisfied:</span>
                  <span className="font-bold text-green-400">{soft.satisfied}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray">Relaxed/Violated:</span>
                  <span className="font-bold text-yellow-400">{soft.violated}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-gray">Compliance Rate:</span>
                  <span className="font-bold text-primary">{soft.complianceRate}%</span>
                </div>
              </div>
              <div className="mt-4 h-2 bg-dark-600 rounded-full overflow-hidden">
                <div className="h-full bg-yellow-500" style={{ width: `${soft.complianceRate}%` }} />
              </div>
            </div>
          </div>
        </motion.div>

        {/* Hard Constraints Detail */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }} className="card mb-6">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-2xl font-bold">Hard Constraints</h2>
            <span className="text-sm text-gray">These must never be violated for a valid solution</span>
          </div>
          <div className="space-y-4">
            {hard.details.length > 0 ? hard.details.map((constraint, index) => (
              <div key={index} className="bg-dark-600 rounded-lg p-4">
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-3">
                    {constraint.status === 'satisfied' ? (
                      <CheckCircle className="w-6 h-6 text-green-400" />
                    ) : (
                      <AlertTriangle className="w-6 h-6 text-red-400" />
                    )}
                    <div>
                      <h3 className="font-bold">{constraint.name}</h3>
                      <p className="text-sm text-gray">{constraint.description}</p>
                    </div>
                  </div>
                  <span className={`badge ${constraint.status === 'satisfied' ? 'badge-success' : 'badge-error'}`}>
                    {constraint.status === 'satisfied' ? '✓ Satisfied' : '⚠ Violated'}
                  </span>
                </div>
              </div>
            )) : (
              <p className="text-gray text-sm">No hard constraint details available</p>
            )}
          </div>
        </motion.div>

        {/* Soft Constraints Detail */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.2 }} className="card">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-2xl font-bold">Soft Constraints</h2>
            <span className="text-sm text-gray">These are preferences that may be relaxed for better optimization</span>
          </div>
          <div className="space-y-4">
            {soft.details.length > 0 ? soft.details.map((constraint, index) => (
              <div key={index} className="bg-dark-600 rounded-lg p-4">
                <div className="flex items-center justify-between mb-2">
                  <div className="flex items-center gap-3">
                    {constraint.status === 'satisfied' ? (
                      <CheckCircle className="w-6 h-6 text-green-400" />
                    ) : (
                      <AlertTriangle className="w-6 h-6 text-yellow-400" />
                    )}
                    <div>
                      <h3 className="font-bold">{constraint.name}</h3>
                      <p className="text-sm text-gray">{constraint.description}</p>
                    </div>
                  </div>
                  <span className={`badge ${constraint.status === 'satisfied' ? 'badge-success' : 'badge-warning'}`}>
                    {constraint.status === 'satisfied' ? '✓ Satisfied' : '~ Relaxed'}
                  </span>
                </div>
              </div>
            )) : (
              <p className="text-gray text-sm">No soft constraint details available</p>
            )}
          </div>
        </motion.div>
      </div>
    </div>
  );
}
