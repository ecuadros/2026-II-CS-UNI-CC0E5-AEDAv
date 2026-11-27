#ifndef CONTAINERS_BTREEPAGE_H
#define CONTAINERS_BTREEPAGE_H

#include "../core/GeneralIterator.h"
#include "../lists/array.h"
#include "binarytreeiterator.h"
#include <algorithm>
#include <cassert>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

/** Resultado de una operación sobre una página, que la página padre usa para reorganizarse. */
enum class BTreeStatus { ok, overflow, underflow, duplicate, notfound };

/** Página válida para BTree: insertar, eliminar, buscar, posicionarse, dividir la raíz, copiar e intercambiar. */
template <typename P>
concept BTreePageConcept = requires(P& page, const P& cpage, const typename P::value_type& key,
                                    const typename P::ref_type& ref, typename P::Index n) {
    typename P::Node;
    { page.insert(key, ref, true) } -> std::same_as<BTreeStatus>;
    { page.remove(key) } -> std::same_as<BTreeStatus>;
    { cpage.search(key) } -> std::convertible_to<const typename P::Node*>;
    { cpage.seek(key, true) };
    { cpage.size() } -> std::convertible_to<typename P::Index>;
    { cpage.height() } -> std::convertible_to<typename P::Index>;
    { cpage.order() } -> std::convertible_to<typename P::Index>;
    { P::root_capacity(n) } -> std::convertible_to<typename P::Index>;
    page.split_root();
    page.reset();
    page.swap(page);
    P(n, n);
    P(cpage);
};

template <typename Page> struct BTreePosition;

/**
 * Página de un árbol B*: hasta `m_maxKeys` claves ordenadas y una subpágina más que claves.
 * Al desbordarse reparte claves con sus hermanas y, si no puede, divide dos páginas en tres;
 * al quedar por debajo de 2/3 pide claves a sus hermanas y, si no puede, junta tres páginas en dos.
 * @tparam Traits Tipos de clave, referencia, nodo, comparador e índice.
 */
template <typename Traits>
class BTreePage {
public:
    using value_type = typename Traits::value_type;
    using ref_type = typename Traits::ref_type;
    using Compare = typename Traits::Compare;
    using Node = typename Traits::Node;
    using Index = typename Traits::Index;
    using PagePtr = BTreePage*;
    using ConstPagePtr = const BTreePage*;
    using OwnedPage = std::unique_ptr<BTreePage>;
    using Keys = typename Traits::template Storage<Node>;
    using Pages = typename Traits::template Storage<OwnedPage>;
    /** Puntero a página con la misma constancia que @p Self. */
    template <typename Self>
    using PtrFor = std::conditional_t<std::is_const_v<Self>, ConstPagePtr, PagePtr>;

    /** Páginas llenas que se dividen juntas al desbordarse. */
    static constexpr Index split_sources = 2;
    /** Páginas en que quedan divididas; al juntar se hace el camino inverso. */
    static constexpr Index split_targets = 3;
    /** Fracción mínima de llenado de una página: 2/3. */
    static constexpr Index fill_numerator = 2, fill_denominator = 3;
    /** Menor orden con el que dividir dos páginas en tres deja cada una con al menos una clave. */
    static constexpr Index min_order = 3;

    /** Capacidad de la raíz para un árbol de orden @p order: le caben dos páginas llenas y la clave que las separa. */
    static constexpr Index root_capacity(Index order) { return split_sources * order + 1; }

private:
    /** Secuencia de claves con sus subpáginas, una más que claves; se usa para juntar y repartir páginas. */
    struct Run {
        Keys keys;
        Pages pages;
    };

    Keys m_keys;
    Pages m_pages;
    Index m_count = 0;
    Index m_maxKeys;
    /** Máximo de claves de las páginas hijas; solo la raíz lo tiene distinto del suyo. */
    Index m_maxKeysForChilds;
    /** Página padre, o `nullptr` en la raíz; permite a los iteradores subir sin guardar el camino. */
    PagePtr m_parent = nullptr;
    [[no_unique_address]] Compare m_comp;

    /** Hace a esta página padre de @p child, si existe. */
    void adopt(PagePtr child) {
        if (child) child->m_parent = this;
    }
    /** Hace a esta página padre de todas sus subpáginas. */
    void adopt_children() {
        for (Index i = 0; i <= m_count; ++i)
            adopt(m_pages[i].get());
    }

    Bool equivalent(const value_type& a, const value_type& b) const { return !m_comp(a, b) && !m_comp(b, a); }

    /** Posición de la primera clave que no va antes que @p key. */
    Index lower_bound(const value_type& key) const {
        auto first = m_keys.begin();
        return std::lower_bound(first, first + m_count, key,
                                [&](const Node& node, const value_type& k) { return m_comp(node.getValue(), k); }) -
               first;
    }

    /** Posición de la primera clave que va después de @p key. */
    Index upper_bound(const value_type& key) const {
        auto first = m_keys.begin();
        return std::upper_bound(first, first + m_count, key,
                                [&](const value_type& k, const Node& node) { return m_comp(k, node.getValue()); }) -
               first;
    }

    /** Indica si la clave de la posición @p pos es equivalente a @p key. */
    Bool matches(Index pos, const value_type& key) const {
        return pos < m_count && equivalent(key, m_keys[pos].getValue());
    }

    template <typename A, typename X> static void insert_at(A& v, X&& x, Index pos) {
        std::move_backward(v.begin() + pos, v.end() - 1, v.end());
        v[pos] = std::forward<X>(x);
    }

    template <typename A> static void remove_at(A& v, Index pos) {
        std::move(v.begin() + pos + 1, v.end(), v.begin() + pos);
    }

    Bool overflow() const { return m_count > m_maxKeys; }
    Bool underflow() const { return m_count < min_keys(); }
    Bool is_full() const { return m_count >= m_maxKeys; }
    Bool has_spare() const { return m_count > min_keys(); }
    Bool is_root() const { return m_maxKeysForChilds != m_maxKeys; }
    Index min_keys() const { return fill_numerator * m_maxKeys / fill_denominator; }
    Index free_cells() const { return m_maxKeys - m_count; }

    BTreeStatus overflow_status() const { return overflow() ? BTreeStatus::overflow : BTreeStatus::ok; }
    BTreeStatus underflow_status() const { return underflow() ? BTreeStatus::underflow : BTreeStatus::ok; }

    /** Subpágina @p pos, o `nullptr` si esa posición no existe. */
    PagePtr sibling(Index pos) const { return pos >= 0 && pos <= m_count ? m_pages[pos].get() : nullptr; }
    Index keys_of(Index pos) const { return sibling(pos) ? sibling(pos)->m_count : 0; }
    Index free_cells_of(Index pos) const { return sibling(pos) ? sibling(pos)->free_cells() : 0; }

    OwnedPage new_child() const { return std::make_unique<BTreePage>(m_maxKeysForChilds, m_maxKeysForChilds); }

    /** Primera clave del subárbol, que está en su hoja más a la izquierda. */
    Node& first() { return m_pages[0] ? m_pages[0]->first() : m_keys[0]; }

    /** Pasa las claves y subpáginas de @p page al final de @p run, y la deja vacía. */
    static void move_page(PagePtr page, Run& run) {
        for (Index i = 0; i < page->m_count; ++i)
            run.keys.push_back(page->m_keys[i]);
        for (Index i = 0; i <= page->m_count; ++i)
            run.pages.push_back(std::move(page->m_pages[i]));
        page->m_count = 0;
    }

    /** Vacía @p n hijos consecutivos desde @p first en una sola secuencia, con las claves del padre que los separan. */
    Run gather(Index first, Index n) const {
        Run run;
        for (Index k = 0; k < n; ++k) {
            move_page(m_pages[first + k].get(), run);
            if (k + 1 < n) run.keys.push_back(m_keys[first + k]);
        }
        return run;
    }

    /** Reemplaza el contenido por las @p n claves de @p run que empiezan en @p from; devuelve la posición siguiente. */
    Index fill(Run& run, Index from, Index n) {
        for (m_count = 0; m_count < n; ++m_count, ++from) {
            m_keys[m_count] = run.keys[from];
            m_pages[m_count] = std::move(run.pages[from]);
        }
        m_pages[m_count] = std::move(run.pages[from]);
        adopt_children();
        return from;
    }

    /**
     * Reparte @p run en `split_targets` páginas, reutilizando las de @p targets que no sean nulas.
     * @return Las claves que suben al padre y las páginas que quedan entre ellas.
     */
    Run split(Run& run, Pages targets) const {
        Index total = run.keys.size();
        assert(total >= split_sources * (min_order + 1));
        Index share = (total - (split_targets - 1)) / split_targets;
        Run up;
        Index i = 0;
        for (Index k = 0; k < split_targets; ++k) {
            OwnedPage page = targets[k] ? std::move(targets[k]) : new_child();
            i = page->fill(run, i, k + 1 < split_targets ? share : total - i);
            up.pages.push_back(std::move(page));
            if (k + 1 < split_targets) up.keys.push_back(run.keys[i++]);
        }
        return up;
    }

    /** Pone en la posición @p pos las claves y páginas de @p up, en lugar de la clave y las páginas que se dividieron. */
    void place(Index pos, Run& up) {
        m_keys[pos] = up.keys[0];
        m_pages[pos] = std::move(up.pages[0]);
        for (Index k = 1; k < Index(up.keys.size()); ++k, ++m_count) {
            insert_at(m_keys, up.keys[k], pos + k);
            insert_at(m_pages, std::move(up.pages[k]), pos + k);
        }
        m_pages[pos + up.keys.size()] = std::move(up.pages.back());
        for (Index k = 0; k <= Index(up.keys.size()); ++k)
            adopt(m_pages[pos + k].get());
    }

    /** Divide el hijo desbordado @p pos y una hermana llena en `split_targets` páginas. */
    void split_child(Index pos) {
        if (pos > 0 && m_pages[pos - 1]->is_full()) --pos;
        assert(pos < m_count && m_pages[pos + 1]->is_full());
        Run run = gather(pos, split_sources);
        Pages targets(split_targets);
        std::move(m_pages.begin() + pos, m_pages.begin() + pos + split_sources, targets.begin());
        Run up = split(run, std::move(targets));
        place(pos, up);
    }

    /** Pasa claves del hijo @p pos a su hermana izquierda, a través del padre. */
    void redistribute_r2l(Index pos) {
        PagePtr source = m_pages[pos].get(), target = m_pages[pos - 1].get();
        while (source->has_spare() && target->m_count < source->m_count) {
            insert_at(target->m_keys, m_keys[pos - 1], target->m_count++);
            insert_at(target->m_pages, std::move(source->m_pages[0]), target->m_count);
            target->adopt(target->m_pages[target->m_count].get());
            m_keys[pos - 1] = source->m_keys[0];
            remove_at(source->m_keys, 0);
            remove_at(source->m_pages, 0);
            --source->m_count;
        }
    }

    /** Pasa claves del hijo @p pos a su hermana derecha, a través del padre. */
    void redistribute_l2r(Index pos) {
        PagePtr source = m_pages[pos].get(), target = m_pages[pos + 1].get();
        while (source->has_spare() && target->m_count < source->m_count) {
            insert_at(target->m_keys, m_keys[pos], 0);
            insert_at(target->m_pages, std::move(source->m_pages[source->m_count]), 0);
            target->adopt(target->m_pages[0].get());
            ++target->m_count;
            m_keys[pos] = source->m_keys[source->m_count - 1];
            --source->m_count;
        }
    }

    /**
     * Intenta corregir el hijo @p pos con su hermana inmediata. En los extremos mueve @p pos
     * al centro para que redistribute2 pueda usar dos hermanas.
     */
    Bool redistribute1(Index& pos) {
        if (m_pages[pos]->underflow()) {
            Bool fromLeft = keys_of(pos - 1) > keys_of(pos + 1);
            if (!m_pages[fromLeft ? pos - 1 : pos + 1]->has_spare()) {
                if (pos == (fromLeft ? m_count : 0)) pos += fromLeft ? -1 : 1;
                return false;
            }
            fromLeft ? redistribute_l2r(pos - 1) : redistribute_r2l(pos + 1);
            return true;
        }
        Index fcol = free_cells_of(pos - 1), fcor = free_cells_of(pos + 1);
        if (!fcol && !fcor && m_pages[pos]->is_full()) return false;
        fcol > fcor ? redistribute_r2l(pos) : redistribute_l2r(pos);
        return true;
    }

    /** Corrige un hijo con falta de claves usando las dos hermanas de @p pos; si falla solo queda juntar. */
    Bool redistribute2(Index pos) {
        assert(pos > 0 && pos < m_count);
        if (m_pages[pos - 1]->underflow()) {
            redistribute_r2l(pos + 1);
            redistribute_r2l(pos);
            return !m_pages[pos - 1]->underflow();
        }
        if (m_pages[pos + 1]->underflow()) {
            redistribute_l2r(pos - 1);
            redistribute_l2r(pos);
            return !m_pages[pos + 1]->underflow();
        }
        redistribute_l2r(pos - 1);
        redistribute_r2l(pos + 1);
        return !m_pages[pos]->underflow();
    }

    Bool treat_underflow(Index& pos) { return redistribute1(pos) || redistribute2(pos); }

    /** Junta los `split_targets` hijos alrededor de @p pos en `split_sources` páginas. */
    BTreeStatus merge(Index pos) {
        PagePtr c1 = m_pages[pos - 1].get(), c2 = m_pages[pos].get();
        Run run = gather(pos - 1, split_targets);
        assert(Index(run.keys.size()) == split_targets * c2->min_keys() + 1);

        Index i = c1->fill(run, 0, c1->m_maxKeys);
        m_keys[pos - 1] = run.keys[i++];
        remove_at(m_keys, pos);
        remove_at(m_pages, pos + 1);
        --m_count;
        c2->fill(run, i, run.keys.size() - i);
        return underflow_status();
    }

    /** Junta en la raíz sus `split_targets` hijos y las claves que los separan; el árbol pierde un nivel. */
    BTreeStatus merge_root() {
        Run run = gather(0, split_targets);
        fill(run, 0, run.keys.size());
        return BTreeStatus::ok;
    }

public:
    /** Crea una página vacía que admite @p maxKeys claves y cuyos hijos admitirán @p maxKeysForChilds. */
    BTreePage(Index maxKeys, Index maxKeysForChilds)
        : m_keys(maxKeys + 1), m_pages(maxKeys + 2), m_maxKeys(maxKeys),
          m_maxKeysForChilds(maxKeysForChilds) {}

    /** Copia en profundidad la página y sus subpáginas. */
    BTreePage(const BTreePage& other)
        : m_keys(other.m_keys), m_pages(other.m_pages.size()), m_count(other.m_count),
          m_maxKeys(other.m_maxKeys), m_maxKeysForChilds(other.m_maxKeysForChilds) {
        for (Index i = 0; i <= m_count; ++i)
            if (other.m_pages[i]) adopt((m_pages[i] = std::make_unique<BTreePage>(*other.m_pages[i])).get());
    }

    BTreePage& operator=(const BTreePage&) = delete;

    /** Libera las subpáginas y deja la página vacía. */
    void reset() {
        for (OwnedPage& page : m_pages)
            page.reset();
        m_count = 0;
    }

    void swap(BTreePage& other) noexcept {
        std::swap(m_keys, other.m_keys);
        std::swap(m_pages, other.m_pages);
        std::swap(m_count, other.m_count);
        std::swap(m_maxKeys, other.m_maxKeys);
        std::swap(m_maxKeysForChilds, other.m_maxKeysForChilds);
        adopt_children();
        other.adopt_children();
    }

    /** Cantidad de claves de la página. */
    Index size() const { return m_count; }
    /** Máximo de claves de las páginas hijas: el orden del árbol. */
    Index order() const { return m_maxKeysForChilds; }
    /** Clave @p i contando desde la izquierda, o desde la derecha si @p reverse; constante si la página lo es. */
    template <typename Self> auto& key(this Self& self, Index i, Bool reverse) {
        return self.m_keys[reverse ? self.m_count - 1 - i : i];
    }
    /** Subpágina @p i contando desde la izquierda, o desde la derecha si @p reverse. */
    template <typename Self> PtrFor<Self> child(this Self& self, Index i, Bool reverse) {
        return self.m_pages[reverse ? self.m_count - i : i].get();
    }
    /** Página padre, o `nullptr` en la raíz. */
    template <typename Self> PtrFor<Self> parent(this Self& self) { return self.m_parent; }
    /** Posición de la subpágina @p page, contando desde la izquierda o desde la derecha si @p reverse. */
    Index position(ConstPagePtr page, Bool reverse) const {
        Index i = std::find_if(m_pages.begin(), m_pages.begin() + m_count + 1,
                               [page](const OwnedPage& p) { return p.get() == page; }) - m_pages.begin();
        return reverse ? m_count - i : i;
    }

    /** Niveles desde esta página hasta las hojas. */
    Index height() const { return 1 + (m_pages[0] ? m_pages[0]->height() : 0); }

    /**
     * Inserta la clave en la hoja que le corresponde. Si @p unique y ya existe, no inserta.
     * @return `overflow` si esta página quedó con una clave de más.
     */
    BTreeStatus insert(const value_type& key, const ref_type& ref, Bool unique) {
        Index pos = lower_bound(key);
        if (unique && matches(pos, key)) return BTreeStatus::duplicate;
        if (!m_pages[pos]) {
            insert_at(m_keys, Node(key, ref), pos);
            ++m_count;
            return overflow_status();
        }
        BTreeStatus status = m_pages[pos]->insert(key, ref, unique);
        if (status != BTreeStatus::overflow) return status;
        if (!redistribute1(pos)) split_child(pos);
        return overflow_status();
    }

    /**
     * Elimina una clave equivalente a @p key. Si no está en una hoja, la cambia por su sucesora y la borra de ahí.
     * @return `underflow` si esta página quedó con menos claves del mínimo, `notfound` si no existía.
     */
    BTreeStatus remove(const value_type& key) {
        Index pos = lower_bound(key);
        if (matches(pos, key)) {
            if (!m_pages[pos + 1]) {
                remove_at(m_keys, pos);
                --m_count;
                return underflow_status();
            }
            std::swap(m_keys[pos], m_pages[pos + 1]->first());
            ++pos;
        } else if (!m_pages[pos]) {
            return BTreeStatus::notfound;
        }
        BTreeStatus status = m_pages[pos]->remove(key);
        if (status != BTreeStatus::underflow) return status;
        if (treat_underflow(pos)) return BTreeStatus::ok;
        if (is_root() && m_count == split_targets - 1) return merge_root();
        return merge(pos);
    }

    /** Busca una clave equivalente a @p key; `nullptr` si no existe. */
    const Node* search(const value_type& key) const {
        Index pos = lower_bound(key);
        if (matches(pos, key)) return &m_keys[pos];
        return m_pages[pos] ? m_pages[pos]->search(key) : nullptr;
    }

    /**
     * Posición de la primera clave del subárbol que no va antes que @p key o, si @p after, que va después de ella.
     * Sin página en el resultado si no hay ninguna.
     */
    BTreePosition<const BTreePage> seek(const value_type& key, Bool after) const {
        Index pos = after ? upper_bound(key) : lower_bound(key);
        if (m_pages[pos])
            if (auto below = m_pages[pos]->seek(key, after); below.page) return below;
        return pos < m_count ? BTreePosition<const BTreePage>{this, pos} : BTreePosition<const BTreePage>{};
    }

    /** Divide la raíz desbordada en `split_targets` hijos y deja en ella las claves que los separan. */
    void split_root() {
        Run all;
        move_page(this, all);
        Run up = split(all, Pages(split_targets));
        fill(up, 0, up.keys.size());
    }
};

/**
 * Posición de un iterador de árbol B: una página y el índice de una de sus claves. Sin página es el fin del recorrido.
 * @tparam Page Tipo de página, constante o no.
 */
template <typename Page>
struct BTreePosition {
    Page* page = nullptr;
    typename std::remove_const_t<Page>::Index index = 0;
};

/**
 * Base de las políticas de recorrido de un árbol B. Las posiciones se cuentan en el sentido del recorrido,
 * así que el recorrido inverso es el mismo algoritmo en espejo.
 * @tparam Reverse Si es `true`, se recorre de derecha a izquierda.
 */
template <Bool Reverse>
struct BTreeTraversalBase {
    /** Primera posición de @p page, o el fin si es nula o está vacía. */
    template <typename Page> static BTreePosition<Page> start(Page* page) {
        return page && page->size() > 0 ? BTreePosition<Page>{page, 0} : BTreePosition<Page>{};
    }

    /** Hoja a la que se llega bajando siempre por la primera subpágina. */
    template <typename Page> static Page* deepest(Page* page) {
        while (page->child(0, Reverse))
            page = page->child(0, Reverse);
        return page;
    }

    /** Clave siguiente dentro de la misma página, o el fin si era la última. */
    template <typename Page> static BTreePosition<Page> next_key(BTreePosition<Page> pos) {
        return pos.index + 1 < pos.page->size() ? BTreePosition<Page>{pos.page, pos.index + 1} : BTreePosition<Page>{};
    }

    /**
     * Sube desde @p page hasta el primer ancestro para el que @p found, que recibe ese ancestro y la posición
     * de la subpágina por la que se llegó, devuelve una posición válida.
     */
    template <typename Page, typename Found> static BTreePosition<Page> climb(Page* page, Found found) {
        for (Page* parent; (parent = page->parent()); page = parent)
            if (auto pos = found(parent, parent->position(page, Reverse)); pos.page) return pos;
        return {};
    }
};

/**
 * Política de recorrido de un árbol B: `first()` da la posición inicial y `next()` la siguiente,
 * sin pila: para volver usa el puntero de cada página a su padre.
 * @tparam Order Orden del recorrido.
 * @tparam Reverse Si es `true`, recorre en espejo.
 */
template <TreeOrder Order, Bool Reverse>
struct BTreeTraversalPolicy;

/** Inorden: cada clave entre la subpágina anterior y la siguiente. */
template <Bool Reverse>
struct BTreeTraversalPolicy<TreeOrder::InOrder, Reverse> : BTreeTraversalBase<Reverse> {
    using Base = BTreeTraversalBase<Reverse>;

    template <typename Page> static BTreePosition<Page> first(Page* root) {
        return root ? Base::start(Base::deepest(root)) : BTreePosition<Page>{};
    }

    template <typename Page> static BTreePosition<Page> next(BTreePosition<Page> pos) {
        if (Page* right = pos.page->child(pos.index + 1, Reverse)) return {Base::deepest(right), 0};
        if (auto same = Base::next_key(pos); same.page) return same;
        return Base::climb(pos.page, [](Page* parent, auto i) {
            return i < parent->size() ? BTreePosition<Page>{parent, i} : BTreePosition<Page>{};
        });
    }
};

/** Preorden: las claves de la página y luego cada subpágina. */
template <Bool Reverse>
struct BTreeTraversalPolicy<TreeOrder::PreOrder, Reverse> : BTreeTraversalBase<Reverse> {
    using Base = BTreeTraversalBase<Reverse>;

    template <typename Page> static BTreePosition<Page> first(Page* root) { return Base::start(root); }

    template <typename Page> static BTreePosition<Page> next(BTreePosition<Page> pos) {
        if (auto same = Base::next_key(pos); same.page) return same;
        if (Page* down = pos.page->child(0, Reverse)) return {down, 0};
        return Base::climb(pos.page, [](Page* parent, auto i) {
            return i < parent->size() ? BTreePosition<Page>{parent->child(i + 1, Reverse), 0} : BTreePosition<Page>{};
        });
    }
};

/** Postorden: cada subpágina y luego las claves de la página. */
template <Bool Reverse>
struct BTreeTraversalPolicy<TreeOrder::PostOrder, Reverse> : BTreeTraversalBase<Reverse> {
    using Base = BTreeTraversalBase<Reverse>;

    template <typename Page> static BTreePosition<Page> first(Page* root) {
        return root ? Base::start(Base::deepest(root)) : BTreePosition<Page>{};
    }

    template <typename Page> static BTreePosition<Page> next(BTreePosition<Page> pos) {
        if (auto same = Base::next_key(pos); same.page) return same;
        Page* parent = pos.page->parent();
        if (!parent) return {};
        auto i = parent->position(pos.page, Reverse);
        return i < parent->size() ? BTreePosition<Page>{Base::deepest(parent->child(i + 1, Reverse)), 0}
                                  : BTreePosition<Page>{parent, 0};
    }
};

/** Tipo de clave al que apunta un iterador sobre @p Page: constante si la página lo es. */
template <typename Page>
using BTreeKeyOf = std::remove_reference_t<decltype(std::declval<Page&>().key(0, false))>;

/**
 * Iterador de árbol B que avanza con BTreeTraversalPolicy. Solo guarda la posición actual: página e índice.
 * @tparam Page Tipo de página; si es constante, el iterador no permite modificar las claves.
 * @tparam Order Orden del recorrido.
 * @tparam Reverse Si es `true`, recorre en espejo.
 */
template <typename Page, TreeOrder Order, Bool Reverse>
class BTreeIterator : public GeneralIterator<BTreeIterator<Page, Order, Reverse>, BTreeKeyOf<Page>> {
    using Policy = BTreeTraversalPolicy<Order, Reverse>;
    using Parent = GeneralIterator<BTreeIterator, BTreeKeyOf<Page>>;
    BTreePosition<Page> m_pos;

    void go(BTreePosition<Page> pos) {
        m_pos = pos;
        this->m_ptr = pos.page ? &pos.page->key(pos.index, Reverse) : nullptr;
    }

public:
    /** Crea el iterador en la primera clave del recorrido del árbol @p root; con `nullptr` es el iterador de fin. */
    BTreeIterator(Page* root = nullptr) : Parent(nullptr) { go(Policy::first(root)); }

    /** Crea el iterador en una posición concreta; una posición sin página es el iterador de fin. */
    explicit BTreeIterator(BTreePosition<Page> pos) : Parent(nullptr) { go(pos); }

    /** Avanza a la siguiente clave del recorrido. */
    BTreeIterator& operator++() {
        go(Policy::next(m_pos));
        return *this;
    }
};

#endif // CONTAINERS_BTREEPAGE_H
