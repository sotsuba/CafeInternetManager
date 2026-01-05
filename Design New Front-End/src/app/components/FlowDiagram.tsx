import { motion, AnimatePresence } from 'motion/react';

export interface FlowNode {
  id: string;
  label: string;
  subtitle?: string;
  icon: React.ReactNode;
  children?: FlowNode[];
  description?: string;
  details?: string[];
}

interface FlowDiagramProps {
  nodes: FlowNode[];
  selectedPath: string[];
  onNodeClick: (nodeId: string) => void;
  showFlow?: boolean;
  flowStep?: number;
}

export function FlowDiagram({ nodes, selectedPath, onNodeClick, showFlow, flowStep = 0 }: FlowDiagramProps) {
  const renderNode = (node: FlowNode, index: number, isActive: boolean) => {
    const hasChildren = node.children && node.children.length > 0;
    const isExpanded = selectedPath.includes(node.id);

    return (
      <motion.button
        key={node.id}
        initial={{ opacity: 0, scale: 0.8 }}
        animate={{ opacity: 1, scale: 1 }}
        transition={{ delay: index * 0.15, type: 'spring', stiffness: 200 }}
        whileHover={{ scale: 1.05, y: -4 }}
        whileTap={{ scale: 0.98 }}
        onClick={() => onNodeClick(node.id)}
        className={`
          relative flex flex-col items-center gap-4 p-8 rounded-3xl transition-all min-w-[160px]
          ${isActive 
            ? 'bg-gradient-to-br from-gray-900 to-black text-white shadow-2xl' 
            : 'bg-white border-2 border-gray-200 hover:border-gray-400 hover:shadow-lg'
          }
          ${hasChildren && isExpanded ? 'ring-4 ring-blue-300 ring-opacity-50' : ''}
        `}
      >
        <motion.div 
          className={`w-20 h-20 flex items-center justify-center transition-all ${isActive ? 'text-white' : 'text-gray-700'}`}
          animate={isActive ? { rotate: [0, 5, -5, 0] } : {}}
          transition={{ duration: 2, repeat: Infinity }}
        >
          {node.icon}
        </motion.div>
        <div className="text-center">
          <div className={`font-semibold text-lg ${isActive ? 'text-white' : 'text-gray-900'}`}>
            {node.label}
          </div>
          {node.subtitle && (
            <div className={`text-xs mt-1 ${isActive ? 'text-gray-300' : 'text-gray-500'}`}>
              {node.subtitle}
            </div>
          )}
        </div>

        {/* Pulse effect when active */}
        {isActive && (
          <motion.div
            className="absolute inset-0 rounded-3xl bg-blue-500 opacity-20"
            animate={{
              scale: [1, 1.1, 1],
              opacity: [0.2, 0, 0.2],
            }}
            transition={{
              duration: 2,
              repeat: Infinity,
            }}
          />
        )}

        {/* Children indicator */}
        {hasChildren && (
          <div className={`absolute -bottom-2 left-1/2 -translate-x-1/2 w-6 h-6 rounded-full flex items-center justify-center text-xs ${
            isActive ? 'bg-white text-black' : 'bg-gray-200 text-gray-600'
          }`}>
            {node.children!.length}
          </div>
        )}
      </motion.button>
    );
  };

  const renderBidirectionalArrow = (index: number, fromNode: FlowNode, toNode: FlowNode) => {
    const isActive = showFlow && (flowStep === index || flowStep === index + 1);
    const isForward = showFlow && flowStep === index;
    const isBackward = showFlow && flowStep === index + 1;

    return (
      <div key={`arrow-${index}`} className="relative flex items-center justify-center w-24">
        <svg width="96" height="80" viewBox="0 0 96 80" className="relative">
          <defs>
            <marker
              id={`arrowhead-right-${index}`}
              markerWidth="10"
              markerHeight="10"
              refX="9"
              refY="5"
              orient="auto"
            >
              <path 
                d="M0,0 L10,5 L0,10 L3,5 Z" 
                fill={isActive ? '#000' : '#D1D5DB'} 
              />
            </marker>
            <marker
              id={`arrowhead-left-${index}`}
              markerWidth="10"
              markerHeight="10"
              refX="1"
              refY="5"
              orient="auto"
            >
              <path 
                d="M10,0 L0,5 L10,10 L7,5 Z" 
                fill={isActive ? '#000' : '#D1D5DB'} 
              />
            </marker>
          </defs>
          
          {/* Forward arrow (top) */}
          <line
            x1="10"
            y1="32"
            x2="86"
            y2="32"
            stroke={isActive ? '#000' : '#D1D5DB'}
            strokeWidth={isActive ? '3' : '2'}
            markerEnd={`url(#arrowhead-right-${index})`}
            className="transition-all duration-300"
          />
          
          {/* Backward arrow (bottom) */}
          <line
            x1="86"
            y1="48"
            x2="10"
            y2="48"
            stroke={isActive ? '#000' : '#D1D5DB'}
            strokeWidth={isActive ? '3' : '2'}
            markerEnd={`url(#arrowhead-left-${index})`}
            className="transition-all duration-300"
          />

          {/* Gateway label in the middle */}
          <text
            x="48"
            y="60"
            textAnchor="middle"
            className="text-[10px] fill-gray-500 font-medium"
          >
            via Gateway
          </text>
        </svg>

        {/* Animated particles */}
        <AnimatePresence>
          {isForward && (
            <motion.div
              initial={{ x: 0, opacity: 1 }}
              animate={{ x: 96, opacity: [1, 1, 0] }}
              exit={{ opacity: 0 }}
              transition={{ duration: 1.5, ease: 'linear', repeat: Infinity }}
              className="absolute top-[28px] left-0 w-3 h-3 bg-blue-500 rounded-full shadow-lg"
              style={{ boxShadow: '0 0 10px rgba(59, 130, 246, 0.8)' }}
            />
          )}
          {isBackward && (
            <motion.div
              initial={{ x: 96, opacity: 1 }}
              animate={{ x: 0, opacity: [1, 1, 0] }}
              exit={{ opacity: 0 }}
              transition={{ duration: 1.5, ease: 'linear', repeat: Infinity }}
              className="absolute top-[44px] left-0 w-3 h-3 bg-purple-500 rounded-full shadow-lg"
              style={{ boxShadow: '0 0 10px rgba(168, 85, 247, 0.8)' }}
            />
          )}
        </AnimatePresence>
      </div>
    );
  };

  return (
    <div className="flex items-center justify-center gap-2">
      {nodes.map((node, index) => (
        <div key={node.id} className="flex items-center gap-2">
          {renderNode(
            node,
            index,
            selectedPath.includes(node.id) || (showFlow && (flowStep === index || flowStep === index + 1))
          )}
          {index < nodes.length - 1 && renderBidirectionalArrow(index, node, nodes[index + 1])}
        </div>
      ))}
    </div>
  );
}
