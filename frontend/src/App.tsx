import { Routes, Route } from 'react-router-dom';
import Layout from './components/Layout';
import LandingDashboard from './pages/LandingDashboard';
import DataUpload from './pages/DataUpload';
import DataInsights from './pages/DataInsights';
import OptimizationProcessing from './pages/OptimizationProcessing';
import ResultsOverview from './pages/ResultsOverview';
import ConstraintValidation from './pages/ConstraintValidation';
import RouteExplorer from './pages/RouteExplorer';
import VehicleFleet from './pages/VehicleFleet';
import EmployeeAssignments from './pages/EmployeeAssignments';
import ExportReports from './pages/ExportReports';

function App() {
  return (
    <Routes>
      <Route path="/" element={<Layout />}>
        <Route index element={<LandingDashboard />} />
        <Route path="upload" element={<DataUpload />} />
        <Route path="insights" element={<DataInsights />} />
        <Route path="processing" element={<OptimizationProcessing />} />
        <Route path="results" element={<ResultsOverview />} />
        <Route path="constraints" element={<ConstraintValidation />} />
        <Route path="routes" element={<RouteExplorer />} />
        <Route path="fleet" element={<VehicleFleet />} />
        <Route path="employees" element={<EmployeeAssignments />} />
        <Route path="export" element={<ExportReports />} />
      </Route>
    </Routes>
  );
}

export default App;


