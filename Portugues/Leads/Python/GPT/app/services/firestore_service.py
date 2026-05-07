from __future__ import annotations

import csv
import io
import uuid
from collections import Counter
from datetime import UTC, date, datetime, timedelta
from math import ceil

import pandas as pd
from google.cloud.firestore_v1.base_query import FieldFilter

from app.config import get_settings
from app.security import hash_password


def utc_now_iso() -> str:
    return datetime.now(UTC).replace(microsecond=0).isoformat().replace("+00:00", "Z")


class FirestoreService:
    def __init__(self, client) -> None:
        self.client = client
        self.settings = get_settings()
        self.users = self.client.collection(self.settings.users_collection)
        self.leads = self.client.collection(self.settings.leads_collection)

    @staticmethod
    def _normalize_email(email: str) -> str:
        return email.strip().lower()

    @staticmethod
    def _lead_search_blob(payload: dict) -> str:
        parts = [
            payload.get("full_name", ""),
            payload.get("email", ""),
            payload.get("company", ""),
            payload.get("phone", ""),
            payload.get("role", ""),
            payload.get("source", ""),
            payload.get("status", ""),
        ]
        return " ".join(part.strip().lower() for part in parts if part)

    @staticmethod
    def _coerce_date(value) -> date:
        if isinstance(value, date) and not isinstance(value, datetime):
            return value
        if isinstance(value, datetime):
            return value.date()
        return pd.to_datetime(value).date()

    @staticmethod
    def _canonical_status(raw_status: str) -> str:
        cleaned = raw_status.strip().lower().replace(" ", "_")
        aliases = {
            "novo": "novo",
            "new": "novo",
            "em_contato": "em_contato",
            "contacted": "em_contato",
            "qualificado": "qualificado",
            "qualified": "qualificado",
            "perdido": "perdido",
            "lost": "perdido",
        }
        if cleaned not in aliases:
            raise ValueError(f"Status inválido: {raw_status}")
        return aliases[cleaned]

    def _public_user(self, user_id: str, payload: dict) -> dict:
        return {
            "id": user_id,
            "name": payload["name"],
            "email": payload["email"],
            "created_at": payload["created_at"],
        }

    def create_user(self, name: str, email: str, password: str) -> dict:
        normalized_email = self._normalize_email(email)
        if self.get_user_by_email(normalized_email):
            raise ValueError("Já existe um usuário cadastrado com este e-mail.")

        now = utc_now_iso()
        doc_ref = self.users.document()
        payload = {
            "name": name.strip(),
            "email": normalized_email,
            "password_hash": hash_password(password),
            "created_at": now,
            "updated_at": now,
        }
        doc_ref.set(payload)
        return self._public_user(doc_ref.id, payload)

    def get_user_by_email(self, email: str) -> dict | None:
        normalized_email = self._normalize_email(email)
        query = self.users.where(filter=FieldFilter("email", "==", normalized_email)).limit(1)
        documents = list(query.stream())
        if not documents:
            return None
        document = documents[0]
        data = document.to_dict()
        return {"id": document.id, **data}

    def get_user_by_id(self, user_id: str) -> dict | None:
        document = self.users.document(user_id).get()
        if not document.exists:
            return None
        return {"id": document.id, **document.to_dict()}

    def _serialize_interactions(self, interactions: list[dict] | None) -> list[dict]:
        sorted_items = sorted(interactions or [], key=lambda item: item["created_at"], reverse=True)
        return sorted_items

    def _serialize_lead(self, document_id: str, payload: dict) -> dict:
        return {
            "id": document_id,
            "owner_id": payload["owner_id"],
            "full_name": payload["full_name"],
            "email": payload["email"],
            "phone": payload["phone"],
            "company": payload["company"],
            "role": payload["role"],
            "source": payload["source"],
            "status": payload["status"],
            "captured_at": payload["captured_at"],
            "created_at": payload["created_at"],
            "updated_at": payload["updated_at"],
            "interactions": self._serialize_interactions(payload.get("interactions")),
        }

    def create_lead(self, owner_id: str, payload: dict) -> dict:
        now = utc_now_iso()
        lead_payload = {
            "owner_id": owner_id,
            "full_name": payload["full_name"].strip(),
            "email": self._normalize_email(payload["email"]),
            "phone": payload["phone"].strip(),
            "company": payload["company"].strip(),
            "role": payload["role"].strip(),
            "source": payload["source"].strip(),
            "status": self._canonical_status(payload["status"]),
            "captured_at": self._coerce_date(payload["captured_at"]).isoformat(),
            "created_at": now,
            "updated_at": now,
            "interactions": [],
        }
        lead_payload["search_blob"] = self._lead_search_blob(lead_payload)
        doc_ref = self.leads.document()
        doc_ref.set(lead_payload)
        return self._serialize_lead(doc_ref.id, lead_payload)

    def get_lead(self, owner_id: str, lead_id: str) -> dict | None:
        document = self.leads.document(lead_id).get()
        if not document.exists:
            return None
        payload = document.to_dict()
        if payload["owner_id"] != owner_id:
            return None
        return self._serialize_lead(document.id, payload)

    def update_lead(self, owner_id: str, lead_id: str, payload: dict) -> dict | None:
        document_ref = self.leads.document(lead_id)
        document = document_ref.get()
        if not document.exists:
            return None
        existing = document.to_dict()
        if existing["owner_id"] != owner_id:
            return None

        updated_payload = {
            **existing,
            "full_name": payload["full_name"].strip(),
            "email": self._normalize_email(payload["email"]),
            "phone": payload["phone"].strip(),
            "company": payload["company"].strip(),
            "role": payload["role"].strip(),
            "source": payload["source"].strip(),
            "status": self._canonical_status(payload["status"]),
            "captured_at": self._coerce_date(payload["captured_at"]).isoformat(),
            "updated_at": utc_now_iso(),
        }
        updated_payload["search_blob"] = self._lead_search_blob(updated_payload)
        document_ref.set(updated_payload)
        return self._serialize_lead(document.id, updated_payload)

    def delete_lead(self, owner_id: str, lead_id: str) -> bool:
        document_ref = self.leads.document(lead_id)
        document = document_ref.get()
        if not document.exists:
            return False
        payload = document.to_dict()
        if payload["owner_id"] != owner_id:
            return False
        document_ref.delete()
        return True

    def list_leads(
        self,
        owner_id: str,
        page: int = 1,
        page_size: int = 10,
        sort_by: str = "captured_at",
        sort_direction: str = "desc",
        status: str | None = None,
        source: str | None = None,
        captured_from: date | None = None,
        captured_to: date | None = None,
        search: str | None = None,
    ) -> dict:
        query = self.leads.where(filter=FieldFilter("owner_id", "==", owner_id))
        documents = [self._serialize_lead(doc.id, doc.to_dict()) for doc in query.stream()]

        if status:
            status = self._canonical_status(status)
            documents = [item for item in documents if item["status"] == status]

        if source:
            source_lower = source.strip().lower()
            documents = [item for item in documents if item["source"].lower() == source_lower]

        if captured_from:
            start = captured_from.isoformat()
            documents = [item for item in documents if item["captured_at"] >= start]

        if captured_to:
            end = captured_to.isoformat()
            documents = [item for item in documents if item["captured_at"] <= end]

        if search:
            search_lower = search.strip().lower()
            documents = [
                item
                for item in documents
                if search_lower in " ".join(
                    [
                        item["full_name"].lower(),
                        item["email"].lower(),
                        item["company"].lower(),
                        item["phone"].lower(),
                    ]
                )
            ]

        sortable_fields = {
            "full_name",
            "email",
            "company",
            "source",
            "status",
            "captured_at",
            "created_at",
            "updated_at",
        }
        sort_key = sort_by if sort_by in sortable_fields else "captured_at"
        reverse = sort_direction.lower() != "asc"
        documents.sort(key=lambda item: item.get(sort_key, ""), reverse=reverse)

        total = len(documents)
        total_pages = max(1, ceil(total / page_size)) if page_size else 1
        page = max(1, page)
        start_index = (page - 1) * page_size
        end_index = start_index + page_size
        paginated = documents[start_index:end_index]

        return {
            "items": paginated,
            "total": total,
            "page": page,
            "page_size": page_size,
            "total_pages": total_pages,
        }

    def add_interaction(
        self,
        owner_id: str,
        lead_id: str,
        interaction_type: str,
        note: str,
        created_by_id: str,
        created_by_name: str,
    ) -> dict | None:
        document_ref = self.leads.document(lead_id)
        document = document_ref.get()
        if not document.exists:
            return None

        payload = document.to_dict()
        if payload["owner_id"] != owner_id:
            return None

        interaction = {
            "id": uuid.uuid4().hex,
            "interaction_type": interaction_type.strip(),
            "note": note.strip(),
            "created_at": utc_now_iso(),
            "created_by_name": created_by_name,
            "created_by_id": created_by_id,
        }
        interactions = payload.get("interactions", [])
        interactions.append(interaction)
        payload["interactions"] = interactions
        payload["updated_at"] = utc_now_iso()
        document_ref.set(payload)
        return interaction

    def list_interactions(self, owner_id: str, lead_id: str) -> list[dict]:
        lead = self.get_lead(owner_id, lead_id)
        if not lead:
            return []
        return lead["interactions"]

    def get_dashboard_summary(self, owner_id: str) -> dict:
        query = self.leads.where(filter=FieldFilter("owner_id", "==", owner_id))
        leads = [self._serialize_lead(doc.id, doc.to_dict()) for doc in query.stream()]

        status_summary = {"novo": 0, "em_contato": 0, "qualificado": 0, "perdido": 0}
        for lead in leads:
            status_summary[lead["status"]] = status_summary.get(lead["status"], 0) + 1

        today = date.today()
        window_start = today - timedelta(days=13)
        daily_counts = Counter(
            lead["captured_at"]
            for lead in leads
            if window_start.isoformat() <= lead["captured_at"] <= today.isoformat()
        )
        captures_last_14_days = []
        for offset in range(14):
            current_day = window_start + timedelta(days=offset)
            captures_last_14_days.append(
                {"date": current_day.isoformat(), "total": daily_counts.get(current_day.isoformat(), 0)}
            )

        top_sources_counter = Counter(lead["source"] for lead in leads)
        top_sources = [
            {"source": source, "total": total}
            for source, total in top_sources_counter.most_common(5)
        ]

        return {
            "total_leads": len(leads),
            "status_summary": status_summary,
            "captures_last_14_days": captures_last_14_days,
            "top_sources": top_sources,
        }

    def import_leads_from_dataframe(self, owner_id: str, dataframe: pd.DataFrame) -> dict:
        column_aliases = {
            "nome completo": "full_name",
            "nome": "full_name",
            "full_name": "full_name",
            "email": "email",
            "telefone": "phone",
            "phone": "phone",
            "empresa": "company",
            "company": "company",
            "cargo": "role",
            "role": "role",
            "fonte": "source",
            "source": "source",
            "status": "status",
            "data de captura": "captured_at",
            "captured_at": "captured_at",
        }
        normalized_columns = {}
        for original_name in dataframe.columns:
            key = str(original_name).strip().lower()
            if key in column_aliases:
                normalized_columns[original_name] = column_aliases[key]
        dataframe = dataframe.rename(columns=normalized_columns)

        required_columns = {
            "full_name",
            "email",
            "phone",
            "company",
            "role",
            "source",
            "status",
            "captured_at",
        }
        missing = required_columns.difference(set(dataframe.columns))
        if missing:
            raise ValueError(f"Colunas obrigatórias ausentes: {', '.join(sorted(missing))}")

        imported = 0
        skipped = 0
        errors: list[str] = []
        for index, row in dataframe.iterrows():
            row_number = index + 2
            try:
                if row.isna().all():
                    skipped += 1
                    continue

                payload = {
                    "full_name": str(row["full_name"]).strip(),
                    "email": str(row["email"]).strip(),
                    "phone": str(row["phone"]).strip(),
                    "company": str(row["company"]).strip(),
                    "role": str(row["role"]).strip(),
                    "source": str(row["source"]).strip(),
                    "status": self._canonical_status(str(row["status"])),
                    "captured_at": self._coerce_date(row["captured_at"]),
                }
                if any(not value for key, value in payload.items() if key != "captured_at"):
                    skipped += 1
                    errors.append(f"Linha {row_number}: valores obrigatórios vazios.")
                    continue
                self.create_lead(owner_id, payload)
                imported += 1
            except Exception as exc:  # noqa: BLE001
                skipped += 1
                errors.append(f"Linha {row_number}: {exc}")

        return {"imported": imported, "skipped": skipped, "errors": errors[:50]}

    def export_leads(self, leads: list[dict], file_format: str) -> tuple[bytes, str, str]:
        rows = []
        for lead in leads:
            rows.append(
                {
                    "Nome completo": lead["full_name"],
                    "E-mail": lead["email"],
                    "Telefone": lead["phone"],
                    "Empresa": lead["company"],
                    "Cargo": lead["role"],
                    "Fonte": lead["source"],
                    "Status": lead["status"],
                    "Data de captura": lead["captured_at"],
                    "Criado em": lead["created_at"],
                    "Atualizado em": lead["updated_at"],
                }
            )

        dataframe = pd.DataFrame(rows)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

        if file_format == "csv":
            buffer = io.StringIO()
            dataframe.to_csv(buffer, index=False, encoding="utf-8-sig", quoting=csv.QUOTE_MINIMAL)
            content = buffer.getvalue().encode("utf-8-sig")
            return content, "text/csv; charset=utf-8", f"leads_{timestamp}.csv"

        if file_format == "xlsx":
            buffer = io.BytesIO()
            with pd.ExcelWriter(buffer, engine="openpyxl") as writer:
                dataframe.to_excel(writer, sheet_name="Leads", index=False)
            return (
                buffer.getvalue(),
                "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                f"leads_{timestamp}.xlsx",
            )

        raise ValueError("Formato de exportação inválido. Use csv ou xlsx.")
