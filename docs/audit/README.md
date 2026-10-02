# Audit

> 用途：说明架构审查、设计评审与事后分析报告的存放规范。
> 更新时机：审计/审查文档新增或用途变化时。

本目录用于存放架构审查、设计评审、代码审查报告和事后分析（post-mortem）。

- [`AUDIT-000-audit-template.md`](AUDIT-000-audit-template.md)：当前只读审计模板，保留0～8章、10个领域与问题总表；动态清单、Windows Debug门禁、持久证据、历史ID和已知项复核规则供新审计复制填写。
- [`AUDIT-001-code-quality-audit-2026-08-16.md`](AUDIT-001-code-quality-audit-2026-08-16.md)：2026-08-16 审计及复审历史，原发现/状态/风险接受证据以报告第7～8章为准；实施历史见 [归档](../archive/audit-001-code-quality-fix-phases.md)。
- [`AUDIT-002-code-quality-audit-2026-08-31.md`](AUDIT-002-code-quality-audit-2026-08-31.md)：2026-08-31 审计及复审历史，承接既有项；实施历史见 [归档](../archive/audit-002-code-quality-fix-phases.md)，不把初审计数当当前状态。
- [`AUDIT-003-code-quality-audit-2026-09-15.md`](AUDIT-003-code-quality-audit-2026-09-15.md)：2026-09-15 审计及历史闭环证据；实施历史见 [归档](../archive/audit-003-code-quality-fix-phases.md)，后续新反证由新报告携原ID追踪。
- [`AUDIT-004-code-quality-audit-2026-10-02.md`](AUDIT-004-code-quality-audit-2026-10-02.md)：最新审计基线与消费者证据，包含新发现、历史重开及已知引用；原问题/级别/验证范围见第4、8章，修复方向见第5章。

项目当前状态只维护于 [roadmap](../roadmap/roadmap.md)，AUDIT-004 Phase 的完整实施排期与验收只维护于 [current-iteration](../roadmap/current-iteration.md)。历史审计报告保留各自基线，不因建立排期或后续修复自动回写；本索引不重复维护问题数量或关闭状态。
